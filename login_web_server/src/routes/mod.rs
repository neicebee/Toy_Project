// routes 하위 rs 파일들 import
mod auth;
mod todo;

use actix_web::web;
use crate::middleware::auth_middleware::AuthMiddleware; // crate 루트 기준 AuthMiddleware 구조체 import
use self::{auth::{register, login, logout, delete_user, generate_password, verify_token}, todo::list_todos};  // 현재 모듈 내에서 항목 import

// main.rs에서 App::configure로 호출되어 라우트 설정 담당
pub fn init(cfg: &mut web::ServiceConfig) { // web::ServiceConfig를 가변 참조로 받아 설정 변경
    // cfg 서비스 등록
    cfg.service(
        // "/api/register" 경로 설정(post 요청을 register 함수가 처리)
        web::resource("/api/register").route(web::post().to(register))
    ).service(
        // "/api/login" 경로 설정(post 요청을 login 함수가 처리)
        web::resource("/api/login").route(web::post().to(login))
    ).service(
        web::resource("api/auth/verify-token").route(web::post().to(verify_token))
    ).service(
        web::resource("/api/todos").route(web::get().to(list_todos))
        .wrap(AuthMiddleware)
    ).service(
        web::resource("/api/logout").route(web::post().to(logout))
        .wrap(AuthMiddleware)
    ).service(
        // 인증된 본인을 삭제하는 기능이므로 "/api/user" 경로에 delete 요청으로 처리 
        web::resource("/user").route(web::delete().to(delete_user))
        .wrap(AuthMiddleware)
    ).service(
        web::resource("/api/generate-password").route(web::get().to(generate_password))
    );
}