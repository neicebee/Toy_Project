use leptos::*;
use leptos_router::*;
use web_sys::window;
use once_cell::sync::Lazy;
use serde::{Deserialize};
use log::info;
use base64::{engine::general_purpose::STANDARD, Engine as _};

// 라우트 핸들러 컴포넌트들을 임포트할 위치
use crate::pages::register::Register;
use crate::pages::login::Login;

// JWT 페이로드(Claims)에서 필요한 정보만 정의 (만료 시간 확인용)
#[derive(Clone, Debug, Deserialize)]
struct Claims {
    exp: usize, // 토큰 만료 시간(Expiration Time), Unix Timestamp(초)
    // sub: String, // 사용자 이름
}

// 전역 인증 상태 Signal 선언 (RwSignal<Option<String>>: None이면 로그아웃, Some(username)이면 로그인)
// Lazy: 앱 시작 시 딱 한 번만 초기화되도록 지연 로딩
// RwSignal: 읽기/쓰기가 가능한 Signal
pub static AUTH_STATE: Lazy<RwSignal<Option<String>>> = Lazy::new(|| create_rw_signal(None));

// 로컬 스토리지 키 추가 (사용자 이름 저장용)
pub const USERNAME_KEY: &str = "username";
// JWT 토큰 이름 (로컬 스토리지 저장 시 사용할 키 이름)
pub const JWT_TOKEN_KEY: &str = "jwt_token";

fn parse_jwt_claims(token: &str) -> Result<Claims, String> {
    let parts: Vec<&str> = token.split('.').collect(); // 토큰을 '.' 기준으로 분리
    
    if parts.len()!=3 { // JWT는 3 부분으로 구성되어야 함 (Header.Payload.Signature)
        return Err("Invalid JWT format: Expected 3 parts".to_string());
    }

    let payload_base64 = parts[1]; // 두 번째 부분 (인덱스 1)이 페이로드 (Base64Url 인코딩)
    info!("{:?}", payload_base64);
    // Base64Url 디코딩
    let payload_bytes = match STANDARD.decode(payload_base64) { // Base64Url 디코딩 시도
        Ok(bytes) => bytes,
        Err(e) => {
            info!("Failed to decode JWT payload (Base64Url): {:?}", e);
            return Err(format!("Failed to decode JWT payload (Base64Url): {:?}", e))
        }
    };
    info!("{:?}", payload_base64);
    info!("{:?}", payload_bytes);
    // 디코딩된 바이트를 String으로 변환 (JSON 문자열)
    let payload_json_string = match String::from_utf8(payload_bytes) { // UTF-8 디코딩 시도
        Ok(s) => s,
        Err(e) => return Err(format!("Failed to decode JWT payload (UTF8): {:?}", e)), // UTF-8 변환 실패 시 에러 반환
    };

    // JSON 문자열을 Claims 구조체로 파싱
    match serde_json::from_str::<Claims>(&payload_json_string) { // JSON 파싱 시도
        Ok(claims) => Ok(claims), // 파싱 성공 시 Claims 반환
        Err(e) => return Err(format!("Failed to parse JWT claims (JSON): {:?}", e)), // JSON 파싱 실패 시 에러 반환
    }
}

// 로컬 스토리지에서 JWT 토큰 및 사용자 이름 읽어와 인증 상태 초기화하는 함수
pub fn initialize_auth_state() {
    let local_storage = window().and_then(|w| w.local_storage().ok().flatten());
    if let Some(storage) = local_storage {  // 로컬 스토리지 접근 가능 시
        if let Ok(Some(token)) = storage.get_item(JWT_TOKEN_KEY) {  // 로컬 스토리지에서 토큰 가져오기
            if let Ok(Some(username)) = storage.get_item(USERNAME_KEY) {    // 사용자 이름 가져오기
                match parse_jwt_claims(&token) {    // 수동 파싱 함수 호출
                    Ok(claims) => { // 파싱 성공 시 Claims 가져옴
                        let current_time = js_sys::Date::now()/1000.0; // 현재 시간을 초 단위로
                        if (claims.exp as f64)>current_time { // 만료 시간 > 현재 시간 이면 유효
                            // 로컬 스토리지에 저장된 사용자 이름으로 인증 상태 업데이트
                            // 수동 파싱으로 얻은 claims.sub 와 로컬 스토리지 username 일치 확인 (선택 사항, 안전성 향상)
                            // if claims.sub == username { ... }
                            AUTH_STATE.set(Some(username.clone())); // 인증 상태 업데이트 (Some(username))
                            info!("Initialized auth state: logged in as {}", username);
                        } else {
                            // log_out(); // 만료된 토큰이면 로그아웃 처리
                            info!("Initialized auth state: token expired...");
                        }
                    }
                    Err(e) => {
                        // JWT 파싱 실패 시
                        // log_out();
                        info!("Initialized auth state: failed to parse token claims...: {:?}", e);
                    }
                }
            } else {    // 토큰은 있는데 사용자 이름이 없는 경우
                // log_out(); // 불완전한 상태이므로 로그아웃 처리
                info!("Initialized auth state: token found but username missing");
            }
        } else {    // 로컬 스토리지에 토큰이 없는 경우
            AUTH_STATE.set(None);
            info!("Initialized auth state: no token found");
        }
    } else {    // 로컬 스토리지 접근 불가능 시
        AUTH_STATE.set(None);
        info!("Initialized auth state: local storage not available");
    }
}

// 로컬 스토리지에서 JWT 토큰 삭제 및 인증 상태 초기화 함수
pub fn log_out() {
     let local_storage = window().and_then(|w| w.local_storage().ok().flatten());
     if let Some(storage) = local_storage {
         if storage.remove_item(JWT_TOKEN_KEY).is_ok() { // 로컬 스토리지에서 토큰 삭제
            println!("Token removed from local storage."); // 로그
         } else {
             eprintln!("Failed to remove token from local storage."); // 에러 로그
         }
     } else {
         eprintln!("Local storage not available for logout."); // 에러 로그
     }
     AUTH_STATE.set(None);
}

// Root Component (라우팅 설정 포함)
#[component]
pub fn App() -> impl IntoView {
    // App 컴포넌트가 렌더링될 때 인증 상태 Signal을 Context에 제공
    // provide_context 는 자식 컴포넌트들이 use_context 를 통해 이 값을 가져갈 수 있게 함.
    provide_context(*AUTH_STATE);    // AUTH_STATE Signal을 Context에 제공
    view! {
        // 라우터 컴포넌트: 클라이언트 사이드 라우팅 활성화
        <Router>
            // 라우트 정의: 특정 경로에 어떤 컴포넌트를 렌더링할지 매핑
            <Routes>
                // TODO: 나중에 / 경로에 대한 랜딩 페이지 또는 리다이렉트 설정
                // /login 경로에 대한 라우트 설정
                <Route path="/login" view=Login/>
                // TODO: 나중에 /dashboard 또는 /todos 경로에 대한 라우트 설정 (인증 필요)

                // /register 경로에 대한 라우트 설정
                <Route path="/register" view=Register/>

                // 404 Not Found 라우트 설정 (다른 경로와 일치하지 않을 때)
                <Route
                    path="/*" // 일치하지 않는 모든 경로
                    view=|| view! { <h1>"404 Not Found"</h1> } // 간단한 404 페이지 표시
                />
            </Routes>
        </Router>
    }
}