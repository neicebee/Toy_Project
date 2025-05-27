use actix_web::{web, HttpMessage, HttpRequest, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use sqlx::{Row, SqlitePool};
use bcrypt::{hash, verify};
use crate::auth::create_jwt;
use crate::generator::generate_password as generate_random_password_string;    // crate 루트 기준 generate_password import

use std::sync::Arc;
use crate::Denylist;

#[derive(Deserialize)]
pub struct RegisterInfo {
    username: String,
    password: String,
}

// register 핸들러
// 공개 비동기 함수
pub async fn register(pool: web::Data<SqlitePool>, info: web::Json<RegisterInfo>) -> impl Responder {   // DB 풀 객체를 담은 web::Data 익스트랙터, 요청 본문의 json 데이터 RegisterInfo 구조체로 역직렬화, Actix-web 응답 반환
    // password validity process
    let password = &info.password;
    
    // 길이 검사(8글자 이상)
    if password.len()<8 {
        return HttpResponse::BadRequest().body("Password must be at least 8 characters long...");
    }
    
    // 소문자, 대문자, 특수문자 포함 여부 검사
    let mut has_lower = false;
    let mut has_upper = false;
    let mut has_special = false; // 특수 문자는 영숫자가 아닌 문자로 간단하게 검사

    for c in password.chars() {
        if c.is_lowercase() {
            has_lower = true;
        } else if c.is_uppercase() {
            has_upper = true;
        } else if !c.is_alphanumeric() { // 영문자나 숫자가 아니면 특수 문자로 간주
            has_special = true;
        }

        // 세 가지 조건을 모두 만족하면 더 이상 검사할 필요 없음
        if has_lower && has_upper && has_special {
            break;
        }
    }
    
    // 세 가지 조건 중 하나라도 만족하지 않으면 에러 반환
    if !has_lower || !has_upper || !has_special {
        return HttpResponse::BadRequest().body("Password must contain at least one lowercase letter, one uppercase letter, and one special character.");
    }
    
    // password hashing
    let hashed = match hash(&info.password, 10) {   // info 내의 password의 참조를 10라운드로 hashing
        Ok(h) => h, // hashing 성공 시 결과 저장
        Err(_) => return HttpResponse::InternalServerError().body("Error hasing password...")   // hashing 실패 시 500 에러 응답 반환
    };
    
    // hashing password와 user infomation DB 삽입
    match sqlx::query("insert into users(username, password_hash) values (?, ?)").bind(&info.username).bind(&hashed)    // 쿼리 내의 placeholder에 문자열 바인딩 시 체인 형식으로 바인딩해야 함
        .execute(pool.get_ref()).await {    // DB 풀 참조를 사용해 쿼리 실행 후 완료 대기
            Ok(_) => HttpResponse::Ok().body("User registered!"),   // 삽입 성공 시 200 OK 응답
            Err(e) => {
                eprintln!("{:?}", e);
                HttpResponse::BadRequest().body("Username already exists or DB Error...")   // 삽입 실패 시 400 Bad Request 응답
            }
    }
}

#[derive(Serialize)]
struct LoginSuccessResponse {
    token: String,
    username: String,
}

#[derive(Deserialize)]
pub struct LoginInfo {
    username: String,
    password: String,
}

// login 핸들러
// 공개 비동기 함수
pub async fn login(pool: web::Data<SqlitePool>, info: web::Json<LoginInfo>, denylist: web::Data<Arc<Denylist>>) -> impl Responder {
    // username으로 DB에서 사용자의 password_hash 조회
    let row = match sqlx::query("select password_hash from users where username=?").bind(&info.username)    // 쿼리 바인딩
        .fetch_one(pool.get_ref()).await {  // fetch_one(): 쿼리 결과 중 첫 번째 행만 획득
            Ok(r) => r, // 사용자 존재 시 결과 행 저장
            Err(_) => return HttpResponse::Unauthorized().body("Invalid username or password..."),  // 사용자가 없거나 DB 에러 시 401 Unauthorized 응답 반환
    };
    
    // 입력 비밀번호와 DB 저장 해시값 비교(검증)
        // 해시는 단방향 암호화이기 때문에 동일한 메시지는 동일한 다이제스트를 가짐
    if verify(&info.password, (&row).get("password_hash")).unwrap_or(false) {   // verify() & unwrap_or(false): 입력 password의 참조와 DB 해시의 참조 비교 후 결과(bool) 획득
        denylist.0.lock().unwrap().remove(&info.username.to_string());
        // 비밀번호 검증 성공 시 JWT 토큰 생성
        match create_jwt(&info.username) {  // username에 대한 JWT 생성
            Ok(token) => {
                println!("User {} logged in successfully!", &info.username);
                let response_data = LoginSuccessResponse {
                    token: token,
                    username: info.username.clone(),
                };
                HttpResponse::Ok().json(response_data)  // 토큰 생성 성공 시 토큰을 포함한 json 객체와 200 OK 응답
            }
            Err(_) => {
                eprintln!("Error creating JWT for user {}...", &info.username);
                HttpResponse::InternalServerError().body("Error creating token...")  // 토큰 생성 실패 시 500 에러 응답 반환
            }
        }
    } else {
        eprintln!("Login failed invalid password for user: {}", &info.username);
        HttpResponse::Unauthorized().body("Invalid username or password...")    // 비밀번호 검증 실패 시 401 Unauthorized 응답 반환
    }
}

// logout 핸들러
pub async fn logout(req: HttpRequest, denylist: web::Data<Arc<Denylist>>) -> impl Responder {
    let temp_extensions = req.extensions();
    // RequestExtensions에서 인증된 사용자 이름 얻기
    let username = match temp_extensions.get::<String>() {
        Some(username) => username,
        None => {
            // AuthMiddleware를 거치지 않았거나 설정 오류
            return HttpResponse::InternalServerError().body("Authentication context missing...");
        }
    };
    println!("{}", username);
    
    // Authorization 헤더에서 현재 사용된 토큰 추출
    // (Denylist에 토큰 문자열 자체를 넣거나 사용자 이름과 함께 관리하기 위해 필요)
    // 사용자 이름 자체를 Denylist에 추가하여, 해당 사용자의 모든 토큰을 무효화하는 방식으로 구현합니다.
    // 이는 해당 사용자가 다시 로그인하기 전까지는 어떤 유효한 토큰으로도 접근이 불가능하게 합니다.

    // Mutex Lock 획득 후 Denylist에 사용자 이름 추가
    // if 문 블록 이탈 시 Mutex Lock 해제
    if denylist.0.lock().unwrap().insert(username.to_string()) { // HashSet에 사용자 이름 삽입. 삽입 성공 시 true 반환.
        HttpResponse::Ok().body("Logged out successfully...") // 삽입 성공 (새로 무효화)
    } else {
        HttpResponse::Ok().body("Already logged out or invalid token...") // 이미 무효화되어 있었음
    }
}

// delete 핸들러
pub async fn delete_user(pool: web::Data<SqlitePool>, denylist: web::Data<Arc<Denylist>>, req: HttpRequest) -> impl Responder {
    let temp_extenstions = req.extensions();
    // RequestExtension에서 인증된 사용자 이름 얻기
    let username = match temp_extenstions.get::<String>() {
        Some(username) => username,
        None => {
            // AuthMiddleware를 거치지 않았거나 설정 오류
            return HttpResponse::InternalServerError().body("Authentication context missing...");
        }
    };
    
    println!("{}", username);
    
    // DB에서 사용자 삭제 쿼리 실행
    match sqlx::query("delete from users where username=?").bind(username)
        .execute(pool.get_ref()).await {
            Ok(result) => {
                // 삭제된 행 수 확인
                if result.rows_affected()>0 { // 사용자가 존재했을 경우
                    println!("ok!");
                    // 해당 사용자의 모든 토큰 무효화
                    denylist.0.lock().unwrap().insert(username.clone());
                    HttpResponse::Ok().body("User deleted successfully.")
                } else {    // 사용자가 이미 없었거나 잘못된 사용자 이름이었다면
                    HttpResponse::NotFound().body("User not found in database...")
                }
            }    
            Err(_) => {
                HttpResponse::InternalServerError().body("Database error during deletion...")
            }   
        }
}

// generate_password 핸들러
pub async fn generate_password() -> impl Responder {
    let password = generate_random_password_string();
    HttpResponse::Ok().json(serde_json::json!({"password": password}))
}