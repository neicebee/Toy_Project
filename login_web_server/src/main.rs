mod auth;   // src/auth.rs 사용
mod routes; // src/routes 모듈 import
mod middleware; // src/middleware 모듈 import
mod generator;  // src/generator 모듈 import

use actix_web::{web, App, HttpServer};
use actix_cors::Cors;
use anyhow::{self, Result};
use sqlx::{migrate::Migrator, SqlitePool};
use dotenv::dotenv;
use std::env;

// Denylist 상태 관리를 위한 모듈
use std::sync::{Arc, Mutex};
use std::collections::HashSet;  // 무효화 토큰 및 사용자 이름 저장

// bcrypt 자가시험을 위한 모듈
use bcrypt::{hash_with_salt, Version::TwoB};

// sqlx 마이그레이터 정의
// 컴파일 타임에 ./migrations 폴더를 읽음
static MIGRATOR: Migrator = sqlx::migrate!("./migrations");

// Denylist 타입 정의
// Mutex로 인해 여러 스레드에서 안전하게 접근 가능, Arc로 인해 객체를 여러 곳에서 공유 가능
#[derive(Debug)]
pub struct Denylist(Mutex<HashSet<String>>);

#[actix_web::main]
async fn main() -> Result<()> {
    dotenv().ok();  // .env 파일 읽고 환경 변수로 로드
    bcrypt_self_test().map_err(|e| e)?; // bcrypt 자가시험 실행 후 에러 시 서버 즉시 중단
    println!("Starting server...");
    // DB url 정의
    let db_url = env::var("DATABASE_URL").expect("DATABASE_URL not set in .env or environment...");
    // DB 연결 풀 생성: db_url 경로의 sqlite 파일을 찾거나 새로 만들고 연결 풀 생성
        // await로 비동기 완료 대기
    let pool = SqlitePool::connect(&db_url).await?;
    println!("DB connection successful!");
    
    // Execute Migration
        // sqlx_migrations 테이블 확인 후 적용되지 않은 마이그레이션 스크립트 실행
    MIGRATOR.run(&pool).await?;
    
    // 객체 생성
    let denylist = Arc::new(Denylist(Mutex::new(HashSet::new())));
    let moved_denylist = denylist.clone();  // 복제하여 핸들러나 미들웨어에서 web::Data<Arc<Denylist>> 형태로 접근 가능
    
    // HTTP 서버 생성 및 구동
    println!("Starting HTTP server at 127.0.0.1:8080");
    HttpServer::new(move || {
        // Cors 미들웨어 설정
        let cors = Cors::default()
            .allow_any_origin() // 어떤 출처로부터 오는 요청이든 허용
            .allow_any_method() // 어떤 메서드든 허용
            .allow_any_header() // 어떤 헤더든 허용
            .max_age(3600); // Cors 사전 요청(Preflight Request) 결과 캐싱 시간 설정
        App::new()
            .wrap(cors) // 보통 cors 미들웨어를 타 미들웨어보다 먼저 적용
            // app_data를 통해 핸들러 함수에서 web::Data<SqlitePool>로 접근 가능
            .app_data(web::Data::new(pool.clone())) // 풀을 복제하여 App 인스턴스마다 풀 공유
            .app_data(web::Data::new(moved_denylist.clone()))   // Denylist 공유
            .configure(routes::init)    // routes 모듈의 init 함수를 호출하여 라우트 및 서비스 설정
    }).bind("127.0.0.1:8080")?.run().await?;
    
    Ok(())
}

fn bcrypt_self_test() -> Result<()> {
    println!("Running bcrypt self-test...");
    let test_pw = env::var("BCRYPT_TEST_PASSWORD").expect("BCRYPT_TEST_PASSWORD not set in .env");
    let test_fixed_salt = env::var("BCRYPT_TEST_FIXED_SALT")
        .expect("BCRYPT_TEST_FIXED_SALT not set in .env");
    let expected_hash = env::var("BCRYPT_EXPECTED_HASH").expect("BCRYPT_EXPECTED_HASH not set in .env");
    let test_fixed_salt = test_fixed_salt.as_bytes();
    match hash_with_salt(&test_pw, 10, test_fixed_salt) {
        Ok(result) => {
            if result.format_for_version(TwoB)==expected_hash {
                println!("bcrypt self-test done!");
                Ok(())
            } else {
                let error_msg = "bcrypt self-test failed... Aborting server start...";
                return Err(anyhow::anyhow!("{}", error_msg))
            }
        }
        Err(_) => {
            let error_msg = "bcrypt self-test failed during hashing... Aborting server start...";
            return Err(anyhow::anyhow!("{}", error_msg))
        }
    }
}