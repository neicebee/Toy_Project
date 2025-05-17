use actix_web::{HttpMessage, HttpRequest, HttpResponse, dev::{Service, ServiceRequest, ServiceResponse, Transform}, Error};
use std::rc::Rc;
use crate::auth::decode_jwt;

pub struct AuthMiddleware;

impl<S, B> Transform<S, ServiceRequest> for AuthMiddleware where
S: Service<ServiceRequest, Response=ServiceResponse<B>, Error=Error> + 'static,
S::Future: 'static, {
    type Transform = AuthMiddlewareService<S>;
    type Error = Error;
    type InitError = ();
    fn new_transform(&self, service: S) -> Result<Self::Transform, Self::InitError> {
        Ok(AuthMiddlewareService {
            service: Rc::new(service)
        })
    }
}

pub struct AuthMiddlewareService<S> {
    service: Rc<S>,
}