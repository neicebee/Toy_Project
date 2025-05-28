use leptos::*;
use leptos_router::*;
use web_sys::window;
use once_cell::sync::Lazy;
use serde::{Serialize, Deserialize};
use log::info;
use reqwest;

// 라우트 핸들러 컴포넌트들을 임포트할 위치
use crate::pages::register::Register;
use crate::pages::login::Login;

// 백엔드 verify_token 응답 구조체 정의
#[derive(Clone, Debug, Deserialize, Serialize)] // Serialize 속성 추가 (요청 본문으로도 사용 가능하게)
pub struct VerifyTokenResponse { // pub 으로 공개 (다른 모듈에서도 사용 가능하게)
    pub valid: bool,
    pub username: Option<String>,
}

// 백엔드 verify_token 요청 본문 구조체 정의
#[derive(Clone, Debug, Deserialize, Serialize)] // Deserialize, Serialize 속성 필요
pub struct VerifyTokenRequest { // pub 으로 공개
    pub token: String,
}

// 전역 인증 상태 Signal 선언 (RwSignal<Option<String>>: None이면 로그아웃, Some(username)이면 로그인)
// Lazy: 앱 시작 시 딱 한 번만 초기화되도록 지연 로딩
// RwSignal: 읽기/쓰기가 가능한 Signal
pub static AUTH_STATE: Lazy<RwSignal<Option<String>>> = Lazy::new(|| create_rw_signal(None));

// 로컬 스토리지 키 추가 (사용자 이름 저장용)
pub const USERNAME_KEY: &str = "username";
// JWT 토큰 이름 (로컬 스토리지 저장 시 사용할 키 이름)
pub const JWT_TOKEN_KEY: &str = "jwt_token";

// 로컬 스토리지에서 JWT 토큰 및 사용자 이름 읽어와 인증 상태 초기화하는 함수
pub fn initialize_auth_state() {
    info!("Initializing auth state...");
    let local_storage = window().and_then(|w| w.local_storage().ok().flatten());
    if let Some(storage) = local_storage {  // 로컬 스토리지 접근 가능 시
        if let Ok(Some(token)) = storage.get_item(JWT_TOKEN_KEY) {  // 로컬 스토리지에서 토큰 가져오기
            if let Ok(Some(username)) = storage.get_item(USERNAME_KEY) {    // 사용자 이름 가져오기
                info!("Found token and username in local storage. Verifying with backend..."); // 로그
                // 비동기로 백엔드 API 호출
                spawn_local(async move {
                    let client = reqwest::Client::new();
                    // TODO: 백엔드 주소를 설정에서 가져오도록 개선
                    let verify_url = "http://127.0.0.1:8080/api/auth/verify-token"; // 백엔드 토큰 검증 API 절대 경로

                    let request_body = VerifyTokenRequest { token: token.clone() }; // 요청 본문 생성
                    let res = client.post(verify_url) // POST 요청
                        .json(&request_body) // 요청 본문으로 VerifyTokenRequest JSON 직렬화
                        .send()
                        .await;

                    match res {
                        Ok(response) => {
                            let status = response.status();
                            if status.is_success() { // 2xx 응답 (백엔드가 검증 결과를 보냈음)
                                match response.json::<VerifyTokenResponse>().await { // 응답 본문 JSON 파싱
                                    Ok(verification_result) => {
                                        if verification_result.valid { // 백엔드 검증 결과 valid: true
                                            // 백엔드 응답의 사용자 이름과 로컬 스토리지 사용자 이름 일치 확인 (선택 사항)
                                            if verification_result.username.as_deref() == Some(&username) { // username 일치 확인
                                                AUTH_STATE.set(Some(username.clone())); // 인증 상태 업데이트 (로컬 스토리지 username 사용)
                                                info!("{:?}", AUTH_STATE.get().is_some());
                                                info!("Backend verified token. Logged in as {}", username);
                                            } else {
                                                // 토큰은 유효하나 사용자 이름 불일치 또는 누락
                                                log_out();
                                                info!("Backend verified token, but username mismatch or missing in response.");
                                            }

                                        } else { // 백엔드 검증 결과 valid: false (토큰 만료, 위변조 등)
                                            log_out(); // 로그아웃 처리
                                            info!("Backend verified token as invalid.");
                                        }
                                    }
                                    Err(err) => {
                                        // 백엔드 응답 파싱 실패 시
                                        log_out();
                                        info!("Failed to parse backend verify token response: {:?}", err);
                                    }
                                }
                            } else { // 4xx, 5xx 응답 (백엔드 자체 오류)
                                log_out();
                                let body_text = response.text().await.unwrap_or_default();
                                info!("Backend verify token API returned error status {}: {}", status, body_text);
                            }
                        }
                        Err(err) => { // 백엔드 API 호출 자체 실패
                            log_out();
                            info!("Failed to call backend verify token API: {:?}", err);
                        }
                    }
                }); // 비동기 블록 끝. 이 초기화 함수는 비동기 호출을 시작만 하고 즉시 리턴함.
                // 초기 인증 상태는 비동기 호출 완료 후에 업데이트될 것임.
            } else {    // 토큰은 있는데 사용자 이름이 없는 경우
                log_out(); // 불완전한 상태이므로 로그아웃 처리
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
        info!("Token removed from local storage."); // 로그
        } else {
            info!("Failed to remove token from local storage."); // 에러 로그
        }
    } else {
        info!("Local storage not available for logout."); // 에러 로그
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