use actix_web::{web, HttpResponse, Responder};
use serde::Deserialize;
use sqlx::{Row, SqlitePool};
use bcrypt::{hash, verify};
use crate::auth::create_jwt;

#[derive(Deserialize)]
pub struct RegisterInfo {
    username: String,
    password: String,
}

// register 핸들러
// 공개 비동기 함수
pub async fn register(pool: web::Data<SqlitePool>, info: web::Json<RegisterInfo>) -> impl Responder {   // DB 풀 객체를 담은 web::Data 익스트랙터, 요청 본문의 json 데이터 RegisterInfo 구조체로 역직렬화, Actix-web 응답 반환
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

#[derive(Deserialize)]
pub struct LoginInfo {
    username: String,
    password: String,
}

// login 핸들러
// 공개 비동기 함수
pub async fn login(pool: web::Data<SqlitePool>, info: web::Json<LoginInfo>) -> impl Responder {
    // username으로 DB에서 사용자의 password_hash 조회
    let row = match sqlx::query("select password_hash from users where username=?").bind(&info.username)    // 쿼리 바인딩
        .fetch_one(pool.get_ref()).await {  // fetch_one(): 쿼리 결과 중 첫 번째 행만 획득
            Ok(r) => r, // 사용자 존재 시 결과 행 저장
            Err(_) => return HttpResponse::Unauthorized().body("Invalid username or password..."),  // 사용자가 없거나 DB 에러 시 401 Unauthorized 응답 반환
    };
    
    // 입력 비밀번호와 DB 저장 해시값 비교(검증)
        // 해시는 단방향 암호화이기 때문에 동일한 메시지는 동일한 다이제스트를 가짐
    if verify(&info.password, (&row).get("password_hash")).unwrap_or(false) {   // verify() & unwrap_or(false): 입력 password의 참조와 DB 해시의 참조 비교 후 결과(bool) 획득
        // 비밀번호 검증 성공 시 JWT 토큰 생성
        match create_jwt(&info.username) {  // username에 대한 JWT 생성
            Ok(token) => HttpResponse::Ok().json(serde_json::json!({"token": token})),  // 토큰 생성 성공 시 토큰을 포함한 200 OK 응답
            Err(_) => HttpResponse::InternalServerError().body("Error creating token..."),  // 토큰 생성 실패 시 500 에러 응답 반환
        }
    } else {
        HttpResponse::Unauthorized().body("Invalid username or password...")    // 비밀번호 검증 실패 시 401 Unauthorized 응답 반환
    }
}