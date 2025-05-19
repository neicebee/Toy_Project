// routes 하위 rs 파일들 import
mod auth;
mod todo;

use actix_web::web;
use crate::middleware::auth_middleware::AuthMiddleware; // crate 루트 기준 AuthMiddleware 구조체 import
use self::{auth::{register, login}, todo::list_todos};  // 현재 모듈 내에서 항목 import

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
        // "/api" 경로를 기준으로 하는 scope(묶음) 설정
        // "/api" 스코프 정의(스코프 내의 모든 라우트 경로는 "/api"로 시작
        // 해당 스코프 내의 모든 서비스에 AuthMiddleware 적용
        web::scope("/api").wrap(AuthMiddleware).service(
            // 스코프 내의 하위 서비스를 등록
                // "/todos" 경로 설정(get 요청을 list_todos 함수가 처리)
            web::resource("/todos").route(web::get().to(list_todos))
        )
    );
}