use actix_web::{web, App, HttpServer};
use simple_web_server::{hello, greet_handler};
use anyhow::Result;

// actix-web 비동기 런타임 진입점 매크로
#[actix_web::main]
async fn main() -> Result<()> { // 비동기 함수
    // 새 HTTP 서버 인스턴스 생성
    // 인자를 클로저로 받는데, 해당 클로저는 요청마다 새로운 App 인스턴스를 생성하여 반환
    HttpServer::new(|| {
        // 웹 App 인스턴스 생성
        App::new().service(hello)   // hello 함수를 "/" 경로의 get 요청 핸들러로 등록
        .route("/api/greet", web::get().to(greet_handler))  // greet_handler 함수를 "/api/greet" 경로의 get 요청 핸들러로 등록
        // .route(...) 방식은 .service(...) 방식과 함께 라우트 등록에 사용됨
    }).bind("127.0.0.1:8080")?.run().await?;    // 서버 바인딩 및 서버 실행 후 들어오는 요청 대기(비동기 실행 완료 대기)
    
    Ok(())
}