use leptos::*;
use leptos_router::*; // 라우팅 이동을 위해 훅 사용
use reqwest; // 백엔드 API 호출을 위한 reqwest 임포트
use serde::{Serialize, Deserialize}; // 요청/응답 JSON 처리를 위한 serde 임포트
use web_sys::{Event, MouseEvent, window}; // input 이벤트 타입, button 클릭 이벤트 타입, 창 임포트
use log::info;

use crate::app::{AUTH_STATE, JWT_TOKEN_KEY, USERNAME_KEY};

// backend의 LoginInfo 구조체와 일치하는 구조체 정의
#[derive(Clone, Debug, Serialize, Deserialize)]
struct LoginInfo {
    username: String,
    password: String,
}

// backend login api response 구조체 정의
#[derive(Clone, Debug, Deserialize)]
struct LoginResponse {
    token: String,
    username: String,
}

// Login page component
#[component]
pub fn Login() -> impl IntoView {
    // 컴포넌트 시작 부분에서 인증 상태 확인
    let auth_state = use_context::<RwSignal<Option<String>>>()
        .expect("Auth state context not found");
    if auth_state.get().is_some() { // 인증 상태가 Some (로그인됨) 이면
        info!("Login Guard (Internal): User authenticated, redirecting from login page."); // 로그
        // 인증된 상태이므로 /todos 페이지로 리다이렉트
        return view! { <Redirect path="/todos"/> }.into(); // Redirect 컴포넌트 반환
    }
    // input field 값 상태 관리
    let (username, set_username) = create_signal("".to_string());
    let (password, set_password) = create_signal("".to_string());
    // 로그인 요청 상태 관리 (로딩 중, 성공, 실패 등 표시)
    let (login_status, set_login_status) = create_signal("".to_string());

    // 라우팅 이동을 위한 훅
    let navigate = use_navigate();
    // 로그인 버튼 클릭 이벤트 핸들러
    let on_submit = move |ev: MouseEvent| {
        ev.prevent_default(); // 폼 제출 시 페이지 새로고침 방지 (form 태그 사용 시)

        let user_info = LoginInfo { // 현재 상태 값으로 LoginInfo 구조체 생성
            username: username.get(), // .get()으로 Signal 값 가져옴
            password: password.get(),
        };
        set_login_status.set("Logging in...".to_string()); // 상태 업데이트 (로딩 중 표시)

        // 비동기 로그인 요청 실행
        // async move 블록 안으로 navigate 변수의 복제본을 move 시킵니다.
        let navigate_for_async = navigate.clone(); // navigate 복제
        spawn_local(async move {
            let client = reqwest::Client::new();
            let login_url = "http://127.0.0.1:8080/api/login";

            // 백엔드 /api/login 엔드포인트로 POST 요청
            let res = client.post(login_url)
                .json(&user_info) // LoginInfo 구조체를 JSON 본문으로 자동 직렬화
                .send()
                .await;

            match res {
                Ok(response) => {
                    // 응답 상태 코드 확인
                    let status = response.status();
                    // 응답 본문 텍스트를 먼저 가져와서 파싱 성공 여부와 상관없이 로그/상태에 사용
                    let body_text = response.text().await.unwrap_or_default();

                    if status.is_success() {
                        set_login_status.set("Login successful!".to_string());

                        // 응답 본문 JSON에서 JWT 토큰 파싱
                        match serde_json::from_str::<LoginResponse>(&body_text) {
                            Ok(data) => {
                                info!("로그인 성공! 사용자: {}, 토큰: {}", data.username, data.token); // 콘솔 로그
                                
                                // JWT 토큰 로컬 스토리지 저장 및 인증 상태 업데이트
                                let local_storage = window().and_then(|w| w.local_storage().ok().flatten());
                                if let Some(storage) = local_storage {
                                    // 로컬 스토리지에 토큰 저장
                                    if storage.set_item(JWT_TOKEN_KEY, &data.token).is_ok() {
                                        info!("JWT token saved to local storage.");
                                        if storage.set_item(USERNAME_KEY, &data.username).is_ok() {
                                            info!("Username saved to local storage.");
                                            // 전역 인증 상태 업데이트 (받은 사용자 이름 사용)
                                            AUTH_STATE.set(Some(data.username.clone())); // 받은 사용자 이름(String)으로 Signal 업데이트
                                            info!("Auth state updated: logged in as {}", data.username);
                                        } else {
                                            info!("Failed to save username to local storage.");
                                            // 사용자 이름 저장 실패 시에도 일단 토큰이 있으니 로그인 상태로 간주할지는 정책 나름.
                                            // 여기서는 오류를 기록하고 로그인 상태 업데이트는 username 없이 진행하거나, 아예 실패 처리 가능.
                                        }
                                    } else {
                                        info!("Failed to save JWT token to local storage.");
                                        // 토큰 저장 실패 시 로그인 실패로 처리하는 것이 안전할 수 있음
                                    }
                                } else {
                                    info!("Local storage not available. Cannot save token.");
                                    // 로컬 스토리지 접근 불가능 시 로그인 실패로 처리하는 것이 안전
                                }
                                // 로그인 성공 시 로그인된 화면으로 이동 (예: /todos)
                                navigate_for_async("/todos", Default::default());
                            }
                            Err(err) => {
                                set_login_status.set(format!("Login successful, but failed to parse token...: {:?}", err)); // 파싱 실패
                                info!("로그인 응답 파싱 실패: {:?}", err); // 에러 로그
                            }
                        }
                    } else {
                        set_login_status.set(format!("Login failed...: {} - {}", status, body_text));
                        info!("로그인 실패: {} - {}", status, body_text); // 콘솔 로그
                    }
                }
                Err(err) => {
                    // 요청 자체 실패 (네트워크 오류 등)
                    set_login_status.set(format!("Request failed...: {:?}", err));
                    info!("로그인 요청 실패: {:?}", err); // 에러 로그
                }
            }
        });
    };
    // 컴포넌트 UI 정의
    view! {
        <h1>"Login"</h1> // 페이지 제목
        // TODO: 폼 태그를 사용하여 Enter 키로 submit 가능하게 개선 (나중에)
        <div>
            <label for="username">"Username: "</label>
            <input
                id="username"
                type="text"
                on:input=move |ev: Event| { set_username.set(event_target_value(&ev)); } // 입력 필드 값 변경 핸들러
                prop:value=username // Signal과 바인딩
            />
        </div>
        <div>
            <label for="password">"Password: "</label>
            <input
                id="password"
                type="password" // 패스워드 필드
                on:input=move |ev: Event| { set_password.set(event_target_value(&ev)); } // 입력 필드 값 변경 핸들러
                prop:value=password // Signal과 바인딩
            />
        </div>
        <p></p>
        // 로그인 버튼
        <button on:click=on_submit>"Login"</button>
        // 로그인 요청 상태 표시
        <p>{login_status}</p>
    }
}