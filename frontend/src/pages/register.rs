use leptos::*;
use leptos_router::*;

use reqwest;
use serde::{Serialize, Deserialize};
use log::info;

// backend의 RegisterInfo 구조체와 일치하는 frontend용 구조체 정의
#[derive(Clone, Debug, Serialize, Deserialize)]
struct RegisterInfo {
    username: String,
    password: String,
}

// backend의 generate_password 결과 구조체
#[derive(Clone, Debug, Deserialize)]
struct GeneratePasswordResponse {
    password: String,
}

// Register Page Component
#[component]
pub fn Register() -> impl IntoView {
    // 컴포넌트 시작 부분에서 인증 상태 확인
    let auth_state = use_context::<RwSignal<Option<String>>>() // Context에서 AUTH_STATE Signal 가져옴
        .expect("Auth state context not found"); // Context에 없을 경우 패닉 (설정 오류)
    if auth_state.get().is_some() { // 인증 상태가 Some (로그인됨) 이면
        info!("Register Guard (Internal): User authenticated, redirecting from register page."); // 로그
        // 인증된 상태이므로 /todos 페이지로 리다이렉트
        // view! {} 매크로 안에서 Redirect 컴포넌트를 반환합니다.
        // 이렇게 하면 이 컴포넌트가 렌더링될 때 Redirect 컴포넌트가 대신 렌더링됩니다.
        return view! { <Redirect path="/todos"/> }.into(); // Redirect 컴포넌트 반환
    }
    // input field 값 상태 관리(Signals)
    let (username, set_username) = create_signal("".to_string());
    let (password, set_password) = create_signal("".to_string());
    // register request 상태 관리(로딩 중, 성공, 실패 등 표시)
    let (register_status, set_register_status) = create_signal("".to_string());
    // 패스워드 추천 요청 상태 관리
    let (generate_status, set_generate_status) = create_signal("".to_string());
    
    // 라우팅 이동 훅
    let navigate = use_navigate();
    
    // register button 클릭 이벤트 핸들러
    let on_submit = move |_| {  // 이벤트 핸들러는 Unit ()을 인자로 받지만, 무시하고 _ 사용
        let user_info = RegisterInfo {  // 현재 상태 값으로 구조체 생성
            username: username.get(),
            password: password.get(),
        };
        set_register_status.set("Processing...".to_string());   // 상태 업데이트
        
        // 비동기 회원가입 요청 실행
        // 현재 스레드에서 비동기 태스크 실행(Wasm 환경)
        let navigate_for_async = navigate.clone();
        spawn_local(async move {
            let client = reqwest::Client::new();    // reqwest HTTP Client Generate
            // /api/register POST 요청
            let res = client.post("http://127.0.0.1:8080/api/register")
                .json(&user_info).send().await;
            match res {
                Ok(response) => {
                    let status = response.status(); // 응답 상태 코드 확인
                    let body_text = response.text().await.unwrap_or_default();  // 응답 본문 텍스트로 가져오기
                    if status.is_success() {
                        set_register_status.set(format!("Registration successful! {}", body_text));
                        info!("회원가입 성공!");
                        // 회원가입 성공 시 로그인 페이지로 이동
                        navigate_for_async("/login", Default::default());
                    } else {
                        set_register_status.set(format!("Registration failed...: {} - {}", status, body_text));
                    }
                }
                Err(err) => {
                    set_register_status.set(format!("Request failed...: {:?}", err));
                    info!("회원가입 요청 실패: {:?}", err);
                }
            }
        });
    };
    
    // 패스워드 추천 이벤트 핸들러
    let on_generate_password = move |_| {
        set_generate_status.set("Generating...".to_string());
        
        // 비동기 패스워드 생성 요청 실행
        spawn_local(async move {
            let client = reqwest::Client::new();
            let generate_url = "http://127.0.0.1:8080/api/generate-password";
            
            let res = client.get(generate_url)
                .send().await;
            
            match res {
                Ok(response) => {
                    let status = response.status();
                    if status.is_success() {
                        match response.json::<GeneratePasswordResponse>().await {
                            Ok(data) => {
                                set_password.set(data.password);
                                set_generate_status.set("Password generated!".to_string());
                                info!("패스워드 추천 성공!");
                            }
                            Err(err) => {
                                set_generate_status.set(format!("Failed to parse password...: {:?}", err));
                                info!("패스워드 응답 파싱 실패: {:?}", err);
                            }
                        }
                    } else {
                        let body_text = response.text().await.unwrap_or_default();
                        set_generate_status.set(format!("Password generation failed...: {} - {}", status, body_text));
                        info!("패스워드 추천 실패: {} - {}", status, body_text);
                    }
                }
                Err(err) => {
                    set_generate_status.set(format!("Request failed...: {:?}", err));
                    info!("패스워드 추천 요청 실패: {:?}", err);
                }
            }
        });
    };
    
    // Component UI 정의
    view! {
        <h1>"Register"</h1> // page title
        // 사용자 이름 입력 필드
        <div>
            <label for="username">"User Name: "</label>
            <input
                id="username" type="text"
                // 입력 값 변경 시 username Signal 업데이트
                on:input=move |ev| { set_username.set(event_target_value(&ev)); }   // 입력 필드 값 변경 핸들러
                // input 필드의 현재 값은 username Signal 값
                prop:value=username
            />
        </div>
        // 패스워드 입력 필드 및 추천 버튼
        <div>
            <label for="password">"Password: "</label>
            <input
                id="password" type="password"
                on:input=move |ev| { set_password.set(event_target_value(&ev)); }
                prop:value=password
            />
            <p></p>
            // 패스워드 추천 버튼 추가 및 코드 연동
            <button on:click=on_generate_password>"Password Recommendation"</button>
        </div>
        <p>{generate_status}</p>
        
        // 회원가입 버튼
        <button on:click=on_submit>"Register"</button>
        // 회원가입 요청 상태 표시
        <p>{register_status}</p>
    }
}