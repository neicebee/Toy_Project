use simple_web_server::greet_handler;

use actix_web::{test, App, web};
use serde_json::Value;

// acitx_web에서 제공하는 test 매크로
#[actix_web::test]
async fn test_greet_handler() {
    // 테스트용 서비스 객체 생성
    let app = test::init_service(
        App::new().route("/api/greet", web::get().to(greet_handler))    // 경로에 대한 라우트 설정
    ).await;
    
    // 테스트 get 요청
    let req = test::TestRequest::get().uri("/api/greet?name=Rust").to_request();
    // 요청에 대한 값을 json으로 받아오기
    // 서비스 객체는 소유권이 넘어가면 안되니 참조 값으로 매개변수 전달
    let resp: Value = test::call_and_read_body_json(&app, req).await;
    
    assert_eq!(resp["message"], "Hello, Rust!");
}