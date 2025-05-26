use leptos::*;
use leptos_router::use_navigate;

use reqwest;
use serde::{Serialize, Deserialize};

// backend의 RegisterInfo 구조체와 일치하는 frontend용 구조체 정의
#[derive(Clone, Debug, Serialize, Deserialize)]
struct RegisterInfo {
    username: String,
    password: String,
}

// Register Page Component
#[component]
pub fn Register() -> impl IntoView {
    // input field 값 상태 관리(Signals)
    let (username, set_username) = create_signal("".to_string());
    let (password, set_password) = create_signal("".to_string());
    // register request 상태 관리(로딩 중, 성공, 실패 등 표시)
    let (register_status, set_register_status) = create_signal("".to_string());
    
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
                        println!("회원가입 성공!");
                        // 회원가입 성공 시 로그인 페이지로 이동
                        navigate_for_async("/login", Default::default());
                    } else {
                        set_register_status.set(format!("Registration failed...: {} - {}", status, body_text));
                    }
                }
                Err(err) => {
                    set_register_status.set(format!("Request failed...: {:?}", err));
                    eprintln!("회원가입 요청 실패: {:?}", err);
                }
            }
        });
    };
    
    // Component UI 정의
    view! {
        <h1>"Register"</h1> // page title
        // 사용자 이름 입력 필드
        <div>
            <label for="username">"User Name:"</label>
            <input
                id="username" type="text"
                // 입력 값 변경 시 username Signal 업데이트
                on:input=move |ev| { set_username.set(event_target_value(&ev)); }   // 입력 필드 값 변경 핸들러
                // input 필드의 현재 값은 username Signal 값
                prop:value=username
            />
        </div>
        // 패스워드 입력 필드
        <div>
            <label for="password">"Password:"</label>
            <input
                id="password" type="password"
                on:input=move |ev| { set_password.set(event_target_value(&ev)); }
                prop:value=password
            />
        </div>
        // 패스워드 추천 버튼 추가 및 코드 연동
        
        // 회원가입 버튼
        <button on:click=on_submit>"Register"</button>
        
        // 회원가입 요청 상태 표시
        <p>{register_status}</p>
    }
}