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
use crate::pages::todos::TodosPage;

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

// 초기 로딩 완료 플래그
pub static IS_INITIAL_AUTH_CHECK_COMPLETE: Lazy<RwSignal<bool>> = Lazy::new(|| create_rw_signal(false));

// 전역 인증 상태 Signal 선언 (RwSignal<Option<String>>: None이면 로그아웃, Some(username)이면 로그인)
// Lazy: 앱 시작 시 딱 한 번만 초기화되도록 지연 로딩
// RwSignal: 읽기/쓰기가 가능한 Signal
pub static AUTH_STATE: Lazy<RwSignal<Option<String>>> = Lazy::new(|| create_rw_signal(None));

// 로컬 스토리지 키 추가 (사용자 이름 저장용)
pub const USERNAME_KEY: &str = "username";
// JWT 토큰 이름 (로컬 스토리지 저장 시 사용할 키 이름)
pub const JWT_TOKEN_KEY: &str = "jwt_token";

// 로컬 스토리지에서 JWT 토큰 문자열 가져오는 함수
// 백엔드 보호된 API 호출 시 Authorization 헤더에 사용
pub fn get_jwt_token() -> Option<String> {
    let local_storage = window().and_then(|w| w.local_storage().ok().flatten()); // 로컬 스토리지 접근

    if let Some(storage) = local_storage { // 접근 가능하면
        storage.get_item(JWT_TOKEN_KEY).ok().flatten() // JWT_TOKEN_KEY 로 저장된 아이템 가져오기 (Option<String>)
    } else {
        None // 접근 불가능하면 None 반환
    }
}

// 인증되지 않은 사용자를 로그인 페이지로 리다이렉트하는 가드 함수(코드 재사용성 향상 위함)
fn _guard_unauthenticated_user() -> Result<impl IntoView, impl IntoView> {
    // 초기 인증 상태 확인 작업이 완료되지 않았다면, 가드 로직을 실행하지 않고 잠시 기다림.
    // Login/Register 페이지와 동일하게 로딩 중 메시지 또는 빈 View 반환.
    if !IS_INITIAL_AUTH_CHECK_COMPLETE.get() {
        info!("Auth Required Guard: Initial check not complete. Waiting...");
        // 로딩 중 메시지를 보여주거나 빈 View 를 반환.
        // Suspense 가 상위에 있으면 여기서 로딩 처리를 직접 안 해도 되지만, 안전을 위해 빈 View 반환.
        return Ok(view! { }.into_view()); // 빈 View 반환 (Suspense 가 로딩 처리)
    }

    // 초기 인증 상태 확인 작업이 완료되었다면, AUTH_STATE 값을 가지고 가드 로직 실행
    info!("Auth Required Guard: Initial check complete. Checking AUTH_STATE. Current value: {:?}", AUTH_STATE.get());

    if AUTH_STATE.get().is_some() { // 인증 상태가 Some (로그인됨) 이면 -> 통과
        info!("Auth Required Guard: User authenticated. Proceeding.");
        Ok(view! {}.into_view()) // 빈 View 반환 (해당 라우트 컴포넌트 렌더링 허용)
    } else { // 인증 상태가 None (로그아웃됨) 이면 -> 로그인 페이지로 리다이렉트
        info!("Auth Required Guard: User not authenticated. Redirecting to login.");
        Err(view! { <Redirect path="/login"/> }.into_view()) // /login 페이지로 Redirect
    }
}

// 로컬 스토리지에서 JWT 토큰 및 사용자 이름 읽어와 인증 상태 초기화하는 함수
pub async fn verify_stored_token_with_backend() -> Option<(String, VerifyTokenResponse)> { // (토큰 문자열, 결과) 튜플 반환 (로그아웃 시 토큰 삭제용)
    info!("Verifying stored token with backend...");
    let local_storage = window().and_then(|w| w.local_storage().ok().flatten());
    if let Some(storage) = local_storage {  // 로컬 스토리지 접근 가능 시
        if let Ok(Some(token)) = storage.get_item(JWT_TOKEN_KEY) {  // 로컬 스토리지에서 토큰 가져오기
            // 로컬 스토리지에 토큰이 있다면 백엔드 검증 API 호출
            info!("Found token in local storage. Calling backend verify...");
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
                                info!("Backend verify token API success. Valid: {}", verification_result.valid);
                                return Some((token.clone(), verification_result));
                            }
                            Err(err) => {
                                // 백엔드 응답 파싱 실패 시
                                info!("Failed to parse backend verify token response: {:?}", err);
                                return None;
                            }
                        }
                    } else { // 4xx, 5xx 응답 (백엔드 자체 오류)
                        let body_text = response.text().await.unwrap_or_default();
                        info!("Backend verify token API returned error status {}: {}", status, body_text);
                        return None;
                    }
                }
                Err(err) => { // 백엔드 API 호출 자체 실패
                    info!("Failed to call backend verify token API: {:?}", err);
                    return None;
                }
            }
        } else {    // 로컬 스토리지에 토큰이 없는 경우
            info!("Initialized auth state: no token found");
            return None;
        }
    } else {    // 로컬 스토리지 접근 불가능 시
        info!("Initialized auth state: local storage not available");
        return None;
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
    provide_context(*IS_INITIAL_AUTH_CHECK_COMPLETE);   // IS_INITIAL_AUTH_CHECK_COMPLETE Signal을 Context에 제공
    
    // create_resource: 비동기 함수 (verify_stored_token_with_backend) 를 실행하고 결과 추적
    // 첫 번째 인자: 비동기 함수의 입력을 제공하는 Signal (여기서는 () Signal - 입력 없음)
    // 두 번째 인자: 실행할 비동기 함수
    let initial_auth_check = create_resource(
        || (), // 입력 Signal (초기 로딩 시 한 번만 실행하도록 입력은 () - Unit)
        |_| async move { // 실행할 비동기 함수 (verify_stored_token_with_backend 호출)
            verify_stored_token_with_backend().await // 비동기 함수 호출 결과 대기
        }
    );
    // initial_auth_check 는 Resource<Option<(String, VerifyTokenResponse)>> 타입.
    // .get() 메서드로 현재 상태(Loading, Error, Value)를 가져올 수 있음.
    
    // Resource 결과에 따라 AUTH_STATE 업데이트 Effect 생성
    // create_effect: Signal 값이 변경될 때마다 클로저 실행
    create_effect(move |_| { // Effect 클로저
        // initial_auth_check Resource 의 현재 값(.get())을 읽음.
        // 값 변경 시 Effect 실행. Some(Some(result)) 는 비동기 함수 성공, Some(None) 은 비동기 함수 실패/토큰 없음.
        if let Some(verification_result) = initial_auth_check.get() { // Resource 결과가 준비되면 (로딩 완료)
            if let Some((_token, result_data)) = verification_result { // 비동기 함수가 Some((token, 결과)) 를 반환했다면
                if result_data.valid { // 백엔드 검증 결과 valid: true
                    // 로컬 스토리지에서 사용자 이름 가져와서 AUTH_STATE 업데이트
                    let local_storage = window().and_then(|w| w.local_storage().ok().flatten());
                    if let Some(storage) = local_storage {
                        if let Ok(Some(username)) = storage.get_item(USERNAME_KEY) {
                            // 로컬 스토리지 사용자 이름으로 인증 상태 업데이트
                            AUTH_STATE.set(Some(username));
                            info!("Initial auth state updated: logged in.");
                        } else {
                            // 로컬 스토리지에 사용자 이름이 없는 경우 (있어야 정상)
                            log_out(); // 불완전 상태이므로 로그아웃 처리
                            info!("Error: Backend verified token but username missing in local storage.");
                        }
                    } else {
                        // 로컬 스토리지 접근 불가능 (있어야 정상)
                        log_out(); // 로그아웃 처리
                        info!("Error: Backend verified token but local storage not available.");
                    }
                } else { // 백엔드 검증 결과 valid: false
                    log_out(); // 로그아웃 처리
                    info!("Initial auth state updated: token invalid (backend check)");
                }
            } else { // 비동기 함수가 None 을 반환했다면 (토큰 없거나 검증 실패)
                log_out(); // 로그아웃 처리
                info!("Initial auth state updated: no token found or verification failed");
            }
            IS_INITIAL_AUTH_CHECK_COMPLETE.set(true);   // 초기화 완료 시 플래그 true로 설정
        }
         // else if initial_auth_check.loading().get() {
         //    // 로딩 중 상태는 Suspense 컴포넌트가 처리.
         // } else if initial_auth_check.error().get().is_some() {
         //    // Resource 실행 중 에러 발생 시 처리 (예: 네트워크 오류)
         //    log_out();
         //    eprintln!("Initial auth check resource error: {:?}", initial_auth_check.error().get());
         // }
    });
    
    view! {
        // Suspense 컴포넌트: 비동기 작업 (Resource) 완료 대기
        // fallback: 비동기 작업이 로딩 중일 때 보여줄 콘텐츠
        <Suspense fallback=move || view! { <p>"Loading initial authentication state..."</p> }>
            // 비동기 작업 (Resource) 완료 후 보여줄 콘텐츠 (라우터 포함)
            // 라우터 컴포넌트: 클라이언트 사이드 라우팅 활성화
            <Router>
                // 라우트 정의: 특정 경로에 어떤 컴포넌트를 렌더링할지 매핑
                <Routes>
                    // / 경로에 대한 랜딩 페이지 또는 리다이렉트 설정
                    <Route
                        path="/"
                        view=|| {
                            // Signal 값에 따라 동적으로 라우팅 결정
                            move || { // move || 블록으로 Signal 값 추적
                                // 초기화 완료 전이면 아무것도 렌더링하지 않고 Suspense 가 기다리게 함.
                                if !IS_INITIAL_AUTH_CHECK_COMPLETE.get() { return view! {}.into_view(); }

                                // 초기화 완료 후, 인증 상태에 따라 리다이렉트
                                if AUTH_STATE.get().is_some() { // 로그인됨
                                    info!("/ Guard: User authenticated, redirecting to /todos.");
                                    view! { <Redirect path="/todos"/> }.into_view() // /todos 로 리다이렉트
                                } else { // 로그아웃됨
                                    info!("/ Guard: User not authenticated, redirecting to /login.");
                                    view! { <Redirect path="/login"/> }.into_view() // /login 으로 리다이렉트
                                }
                            }.into_view()
                        }
                    />
                    // /login 경로에 대한 라우트 설정
                    <Route path="/login" view=Login/>
                    
                    // /todos 경로에 대한 라우트 설정 (인증 필요)
                    <Route path="/todos" view=TodosPage/>

                    // /register 경로에 대한 라우트 설정
                    <Route path="/register" view=Register/>

                    // 404 Not Found 라우트 설정 (다른 경로와 일치하지 않을 때)
                    <Route
                        path="/*" // 일치하지 않는 모든 경로
                        view=|| view! { <h1>"404 Not Found"</h1> } // 간단한 404 페이지 표시
                    />
                </Routes>
            </Router>
        </Suspense>
    }
}