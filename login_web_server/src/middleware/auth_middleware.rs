use actix_web::{
    HttpMessage, HttpResponse,
    body::BoxBody,
    dev::{Service, ServiceRequest, ServiceResponse, Transform}, Error
};
use std::rc::Rc;
use futures_util::future::{ready, Ready};
use crate::auth::decode_jwt;

pub struct AuthMiddleware;

impl<S> Transform<S, ServiceRequest> for AuthMiddleware
where
    S: Service<ServiceRequest, Response=ServiceResponse<BoxBody>, Error=Error>+'static,
    S::Future: 'static,
{   
    type Response = ServiceResponse<BoxBody>;
    type Transform = AuthMiddlewareService<S>;
    type Error = Error;
    type InitError = ();
    type Future = Ready<Result<Self::Transform, Self::InitError>>;
    
    fn new_transform(&self, service: S) -> Self::Future {
        ready(Ok(AuthMiddlewareService { service: Rc::new(service) }))
    }
}

pub struct AuthMiddlewareService<S> {
    service: Rc<S>,
}

impl<S> Service<ServiceRequest> for AuthMiddlewareService<S>
where
    S: Service<ServiceRequest, Response=ServiceResponse<BoxBody>, Error=Error>+'static,
{
    type Response = ServiceResponse<BoxBody>;
    type Error = Error;
    type Future = futures_util::future::LocalBoxFuture<'static, Result<Self::Response, Self::Error>>;
    
    fn poll_ready(&self, cx: &mut std::task::Context<'_>) -> std::task::Poll<Result<(), Self::Error>> {
        self.service.poll_ready(cx)
    }
    
    fn call(&self, req: ServiceRequest) -> Self::Future {
        let svc = self.service.clone();
        Box::pin(async move {
            let (request, payload) = req.into_parts();
            // Header에서 Authorization: Bearer <token> 추출
            if let Some(auth_header) = request.headers().get("Authorization") {
                if let Ok(auth_str) = auth_header.to_str() {
                    if let Some(token) = auth_str.strip_prefix("Bearer ") {
                        match decode_jwt(token) {
                            Ok(username) => {
                                // RequestExtensions에 username 저장
                                request.extensions_mut().insert(username);
                                let original_req = ServiceRequest::from_parts(request, payload);
                                return svc.call(original_req).await;
                            }
                            Err(_) => {
                                let response = HttpResponse::Unauthorized().body("Invalid token").map_into_boxed_body();
                                return Ok(ServiceResponse::new(request, response));
                                // return Ok(req.into_response(HttpResponse::Unauthorized().body("Invalid token").into()))
                            }
                        }
                    }
                }
            }
            let response = HttpResponse::Unauthorized().body("Missing or Invalid Authorization header").map_into_boxed_body();
            Ok(ServiceResponse::new(request, response))
            // Ok(req.into_response(HttpResponse::Unauthorized().body("Missing or Invalid Authorization header").into()))
        })
    }
}