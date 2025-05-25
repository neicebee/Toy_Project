use actix_web::{
    HttpMessage, HttpResponse,  // HttpMessage 트레이트(extensions_mut 사용)
    body::BoxBody,  // 응답 본문 타입(미들웨어 응답 본문 통일)
    // dev::* => actix-web 개발 관련 모듈
    dev::{Service, ServiceRequest, ServiceResponse, Transform}, Error
};
use std::rc::Rc;    // Rc(Reference Counting): 다음 서비스를 여러 Service 구현체에서 공유
use futures_util::future::{ready, Ready};   // 비동기 Future 타입 - Service 구현에 사용
use crate::auth::decode_jwt;

use std::sync::Arc;
use crate::Denylist;
use actix_web::web::Data;

// Middleware Factory 구조체(Transform 트레이트 구현)
// 요청마다 새로운 AuthMiddlewareService 인스턴스 생성 역할
pub struct AuthMiddleware;

// Transform 트레이트 구현(AuthMiddleware는 Transform 트레이트를 구현하여 Middleware Factory 역할을 함
impl<S> Transform<S, ServiceRequest> for AuthMiddleware // <S>: Generic Type, 다음 서비스 타입(Service 트레이트 구현체)
where
    // S 조건: Service 트레이트 구현체, ServiceRequest를 요청으로 받으며, 응답은 ServiceResponse<BoxBody>, 에러는 Error 타입, 'static 라이프타임
    S: Service<ServiceRequest, Response=ServiceResponse<BoxBody>, Error=Error>+'static,
    // 다음 서비스의 Future도 'static 라이프타임을 가져야 함
    S::Future: 'static,
{   
    type Response = ServiceResponse<BoxBody>;   // Transform이 처리한 요청에 대한 응답 타입
    type Transform = AuthMiddlewareService<S>;  // Transform이 생성한 Service 구현체 타입
    type Error = Error; // Transform의 에러 타입
    type InitError = ();    // new_transform 함수에서 발생할 수 있는 초기화 에러 타입
    type Future = Ready<Result<Self::Transform, Self::InitError>>;  // new_transform 함수 반환 타입
    
    // Transform 구현체(AuthMiddlewareService)를 비동기적으로 생성
    fn new_transform(&self, service: S) -> Self::Future {
        // 다음 서비스 객체를 Rc로 감싸서 AuthMiddlewareService의 service 필드에 저장
        ready(Ok(AuthMiddlewareService { service: Rc::new(service) }))
    }
}

// Middleware Service 구현체 구조체(실제 요청을 처리하는 역할)
pub struct AuthMiddlewareService<S> {
    // 다음 서비스(핸들러 함수 또는 그 이후의 미들웨어 체인)
    service: Rc<S>,
}

// AuthMiddlewareService 구조체에 대해 Service 트레이트 구현
impl<S> Service<ServiceRequest> for AuthMiddlewareService<S>
where
    S: Service<ServiceRequest, Response=ServiceResponse<BoxBody>, Error=Error>+'static,
{
    type Response = ServiceResponse<BoxBody>;
    type Error = Error;
    // 서비스가 요청 처리 후 반환하는 비동기 작업(Future)의 타입
    // call 메서드가 반환하는 Box::pin(async move {...})의 타입과 일치
    type Future = futures_util::future::LocalBoxFuture<'static, Result<Self::Response, Self::Error>>;
    
    // 요청 처리 준비 상태 확인
    // cx(&mut std::task::Context<'_>): 비동기 컨텍스트
    fn poll_ready(&self, cx: &mut std::task::Context<'_>) -> std::task::Poll<Result<(), Self::Error>> { // Poll<Result(), Self::Error>>: 준비 상태 결과 반환(Poll::Ready or Poll:Pending)
        // 다음 서비스의 poll ready 상태를 그대로 반환(자신이 준비되면 다음 서비스도 준비되어야 하기 때문)
        self.service.poll_ready(cx)
    }
    
    // 요청 처리 로직(비동기 함수)
    fn call(&self, req: ServiceRequest) -> Self::Future {   // 요청 객체를 비동기 작업(Future)으로 반환
        let svc = self.service.clone(); // 다음 서비스 참조 복제(async move 블록 내에서 사용하기 위함)
        Box::pin(async move {   // 비동기 블록(impl Future)을 힙에 할당 후 Pin으로 고정하여 LocalBoxFuture 타입으로 변환
            let (request, payload) = req.into_parts();  // req 객체 분리 후 소유권 이동(request: 요청 정보, payload: 요청 본문 스트림)
            let temp = request.clone();
            // App 데이터에서 Denylist 객체 가져오기
            let denylist = match temp.app_data::<Data<Arc<Denylist>>>() {
                Some(d) => d,
                None => {
                    // Denylist가 App data에 등록되지 않았다면 설정 오류
                    let response = HttpResponse::InternalServerError().body("Server configuration Error...");
                    return Ok(ServiceResponse::new(request, response));
                }
            };
            // Header에서 Authorization: Bearer <token> 추출
            if let Some(auth_header) = temp.headers().get("Authorization") {
                if let Ok(auth_str) = auth_header.to_str() {
                    if let Some(token) = auth_str.strip_prefix("Bearer ") { // 접두사 제거
                        // JWT 토큰 디코딩 및 검증
                        match decode_jwt(token) {
                            Ok(username) => {   // 토큰 유효 시
                                // Mutex 락 획득 및 HashSet에 username 존재 확인
                                // Mutex lock은 if 블록 이탈 시 해제
                                if denylist.0.lock().unwrap().contains(&username) { // Denylist에 사용자 이름이 있을 경우
                                    let response = HttpResponse::Unauthorized().body("Token is invalidated...");
                                    return Ok(ServiceResponse::new(request, response));
                                }
                                // RequestExtensions에 username 저장
                                // 핸들러 함수에서 req.extensions().get::<String>() 등으로 추출해 사용 가능
                                request.extensions_mut().insert(username);
                                // 다음 서비스로 요청 전달
                                let original_req = ServiceRequest::from_parts(request, payload);    // 분리했던 요소들을 재결합해서 객체 생성
                                return svc.call(original_req).await;    // 다음 서비스 호출 및 결과 대기
                            }
                            Err(_) => { // 토큰 검증 실패 시
                                let response = HttpResponse::Unauthorized().body("Invalid token");
                                // 해당 미들웨어의 Service 구현체는 Response = ServiceResponse<BoxBody>, HttpResponse<BoxBody>는 Into<actix_web::dev::Response<BoxBody>> 트레이트를 구현
                                // 때문에 ServiceResponse 객체 생성 시 타입 추론 가능
                                return Ok(ServiceResponse::new(request, response));
                                // return Ok(req.into_response(HttpResponse::Unauthorized().body("Invalid token").into()))
                            }
                        }
                    }
                }
            }
            // 인증 실패 시
            let response = HttpResponse::Unauthorized().body("Missing or Invalid Authorization header");
            Ok(ServiceResponse::new(request, response))
            // Ok(req.into_response(HttpResponse::Unauthorized().body("Missing or Invalid Authorization header").into()))
        })  // async move 블록의 끝(결과: Result<ServiceResponse<BoxBody>, Error>) 
    }   // call 함수의 끝(반환 타입: Self::Future)
}