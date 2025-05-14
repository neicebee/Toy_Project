use actix_web::{get, web, Responder, HttpResponse, HttpRequest};
use serde::Serialize;

// 직렬화(Serialization): 웹 서버에서 응답을 json 형태로 보낼 시 rust의 구조체 데이터를 json 문자열로 변환하는 과정
// serde 크레이트의 속성을 사용하여 구조체를 json 형태로 변환
// #[derive(Deserialize)]: 역직렬화
#[derive(Serialize)]
struct GreetResponse {
    message: String,
}

// 핸들러 함수
#[get("/")]
pub async fn hello(req: HttpRequest) -> impl Responder {
    if let Some(addr) = req.peer_addr() {
        println!("'/': Get Request client: {}", addr);
    } else {
        println!("'/': Get Request from unknown client.");
    }
    HttpResponse::Ok().body("Hello, World!")
}

// web::Query Extractor를 사용하여 요청의 쿼리 파라미터(?key=value&key2=value2) 자동 파싱 후 HashMap 형태로 제작
pub async fn greet_handler(query: web::Query<std::collections::HashMap<String, String>>, req: HttpRequest) -> impl Responder {
    if let Some(addr) = req.peer_addr() {
        println!("'/api/greet': Get Request client: {}", addr);
    } else {
        println!("'/api/greet': Get Request from unknown client.");
    }
    // cloned(): Option 내의 있는 값의 복사본
    // unwrap_or_else(): Option이 Some(value)일 경우 value 추출, None일 경우 매개변수로 전달한 값 반환
    // ||: Closure(이름 없는 함수) 정의
        // 값을 바로 받지 않고 클로저를 받는 이유는 효율성
        // None일 경우 지연 평가될 클로저를 인자로 받아 필요 시 실행
    let name = query.get("name").cloned().unwrap_or_else(|| "Guest".to_string());
    let response = GreetResponse {
        message: format!("Hello, {}!", name),
    };
    HttpResponse::Ok().json(response)
}