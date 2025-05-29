use leptos::*;
use leptos_router::*; // 페이지 이동을 위해 필요
use web_sys::MouseEvent; // 버튼 클릭 이벤트

use crate::app::{AUTH_STATE, IS_INITIAL_AUTH_CHECK_COMPLETE, get_jwt_token, log_out}; // 전역 인증 상태, 인증 함수 수행 상태, 토큰 가져오기 함수, 로그아웃 함수, 키 임포트
use reqwest; // 백엔드 API 호출을 위해 필요
use web_sys::window; // 로컬 스토리지 접근을 위해 필요
use log::info;

#[component]
pub fn TodosPage() -> impl IntoView {
    view! {
        { move || {
            // 초기 인증 상태 확인 작업이 완료되지 않았다면, 로딩 중 메시지 표시
            if !IS_INITIAL_AUTH_CHECK_COMPLETE.get() {
                info!("TodosPage Guard: Initial check not complete. Waiting...");
                return view! { <p>"Loading initial authentication state..."</p> }.into_view(); // 로딩 메시지 View 반환
            }
            
            // 초기 인증 상태 확인 작업이 완료되었다면, AUTH_STATE 값을 확인
            info!("TodosPage Guard: Initial check complete. Checking AUTH_STATE. Current value: {:?}", AUTH_STATE.get());
            if AUTH_STATE.get().is_none() { // 인증 상태가 None (로그아웃됨) 이면 -> 로그인 페이지로 리다이렉트
                info!("TodosPage Guard: User not authenticated. Redirecting to login.");
                return view! { <Redirect path="/login"/> }.into_view(); // /login 페이지로 Redirect View 반환
            }

            // 초기화 완료 및 인증된 상태이면 -> 원래 TodosPage UI 렌더링 계속 진행
            info!("TodosPage Guard: User authenticated. Proceeding to render page UI.");
            // 사용자 이름을 표시하기 위한 Signal
            // auth_state Signal 값을 읽어서 derive_signal 로 username String Signal 생성
            let username_display = create_memo(move |_| { // create_memo: Signal 값 변경 시 자동으로 값을 계산하고 Signal 처럼 동작
                AUTH_STATE.get().map(|u| format!("{} 님", u)).unwrap_or("로그인되지 않음".to_string())
                // auth_state.get() 결과가 Some(username) 이면 "{username} 님" 문자열 생성
                // None 이면 "로그인되지 않음" 문자열 사용
            });
            
            let navigate = use_navigate();
            let navigate_2 = navigate.clone();
            // 로그아웃 버튼 클릭 이벤트 핸들러
            let on_logout = move |ev: MouseEvent| {
                ev.prevent_default();
                info!("Logout button clicked."); // 로그

                // 백엔드 로그아웃 API 호출
                let navigate_for_async = navigate.clone();
                spawn_local(async move { // 비동기 블록
                    let client = reqwest::Client::new();
                    // TODO: 백엔드 주소를 설정에서 가져오도록 개선
                    let logout_url = "http://127.0.0.1:8080/api/logout"; // 백엔드 로그아웃 API 절대 경로
                    // 로컬 스토리지에서 JWT 토큰 가져와 Authorization 헤더에 포함
                    let token_opt = get_jwt_token(); // 토큰 가져오기 헬퍼 함수 사용
                    let request = client.post(logout_url); // POST 요청 생성
                    let request = if let Some(token) = token_opt { // 토큰이 있다면
                        request.bearer_auth(token) // Authorization: Bearer <token> 헤더 추가
                    } else { // 토큰이 없는 경우 (비정상 상태 또는 이미 로그아웃된 상태)
                        request // 토큰 없이 요청 (백엔드에서 401 응답 예상)
                    };

                    let res = request.send().await;
                    match res {
                        Ok(response) => {
                            let status = response.status();
                            if status.is_success() { // 2xx 응답 (로그아웃 성공)
                                info!("Backend logout API successful.");
                                // 클라이언트 측 로그아웃 처리 실행
                                log_out(); // 로컬 스토리지 삭제 및 AUTH_STATE None 설정
                                navigate_for_async("/login", Default::default()); // /login 페이지로 이동
                            } else { // 4xx, 5xx 응답 (로그아웃 실패 또는 인증 실패)
                                let body_text = response.text().await.unwrap_or_default();
                                info!("Backend logout API failed with status {}: {}", status, body_text);
                                // 401 인증 실패일 경우, 이미 로그아웃된 상태일 수 있으므로 클라이언트 측 로그아웃 처리 시도
                                if status == reqwest::StatusCode::UNAUTHORIZED {
                                    info!("Backend logout API returned 401. Assuming already logged out on client side.");
                                    log_out(); // 클라이언트 측 로그아웃 처리
                                    navigate_for_async("/login", Default::default()); // 로그인 페이지로 이동
                                } else {
                                    // 다른 에러일 경우 사용자에게 알림 (로그아웃 실패)
                                    let _ = window().unwrap().alert_with_message(&format!("Logout failed: {}. Please try again.", status)); // alert 대화상자로 알림
                                }
                            }
                        }
                        Err(err) => { // 백엔드 API 호출 자체 실패 (네트워크 오류 등)
                            info!("Failed to call backend logout API: {:?}", err);
                            window().unwrap().alert_with_message(&format!("Logout request failed: {:?}. Please check your network.", err)).unwrap(); // alert 대화상자로 알림
                        }
                    }
                });
            };
            
            // 사용자 탈퇴 버튼 클릭 이벤트 핸들러
            let on_delete_user = move |ev: MouseEvent| {
                ev.prevent_default();
                info!("Delete user button clicked."); // 로그
                // 사용자에게 확인 메시지 표시
                let confirmed = window().unwrap().confirm_with_message("정말 탈퇴하시겠습니까? 이 작업은 되돌릴 수 없습니다.").unwrap_or(false); // confirm 대화상자
                if confirmed { // 사용자가 확인을 눌렀을 경우에만 진행
                    info!("User confirmed deletion. Calling backend API...");

                    // 백엔드 유저 탈퇴 API 호출
                    let navigate_for_async = navigate_2.clone();
                    spawn_local(async move { // 비동기 블록
                        let client = reqwest::Client::new();
                        // TODO: 백엔드 주소를 설정에서 가져오도록 개선
                        let delete_url = "http://127.0.0.1:8080/user"; // 백엔드 유저 탈퇴 API 절대 경로 (DELETE)

                        // 로컬 스토리지에서 JWT 토큰 가져와 Authorization 헤더에 포함
                        let token_opt = get_jwt_token(); // 토큰 가져오기 헬퍼 함수 사용

                        let request = client.delete(delete_url); // DELETE 요청 생성

                        let request = if let Some(token) = token_opt { // 토큰이 있다면
                            request.bearer_auth(token) // Authorization: Bearer <token> 헤더 추가
                        } else { // 토큰이 없는 경우
                            request // 토큰 없이 요청
                        };

                        let res = request.send().await; // 요청 보내고 결과 대기
                        match res {
                            Ok(response) => {
                                let status = response.status();
                                if status.is_success() { // 2xx 응답 (탈퇴 성공)
                                    info!("Backend delete user API successful.");
                                    // 클라이언트 측 사용자 탈퇴 처리 실행
                                    log_out(); // 로컬 스토리지 삭제 및 AUTH_STATE None 설정
                                    navigate_for_async("/login", Default::default()); // /login 페이지로 이동
                                    window().unwrap().alert_with_message("계정 삭제가 완료되었습니다.").unwrap(); // 알림 메시지
                                } else { // 4xx, 5xx 응답 (탈퇴 실패 또는 인증 실패)
                                    let body_text = response.text().await.unwrap_or_default();
                                    info!("Backend delete user API failed with status {}: {}", status, body_text);
                                    // 401 인증 실패일 경우, 이미 탈퇴된 상태일 수 있으므로 클라이언트 측 로그아웃 처리 시도
                                    if status == reqwest::StatusCode::UNAUTHORIZED {
                                        info!("Backend delete user API returned 401. Assuming already deleted on client side.");
                                        log_out(); // 클라이언트 측 로그아웃 처리
                                        navigate_for_async("/login", Default::default()); // 로그인 페이지로 이동
                                        window().unwrap().alert_with_message("계정 삭제가 이미 처리되었거나 인증 정보가 유효하지 않습니다.").unwrap(); // 알림 메시지
                                    } else {
                                        // 다른 에러일 경우 사용자에게 알림 (탈퇴 실패)
                                        window().unwrap().alert_with_message(&format!("계정 삭제에 실패했습니다: {}. 다시 시도해주세요.", status)).unwrap(); // alert 대화상자로 알림
                                    }
                                }
                            }
                            Err(err) => { // 백엔드 API 호출 자체 실패
                                info!("Failed to call backend delete user API: {:?}", err);
                                window().unwrap().alert_with_message(&format!("계정 삭제 요청에 실패했습니다: {:?}. 네트워크 연결을 확인해주세요.", err)).unwrap(); // alert 대화상자로 알림
                            }
                        }
                    });
                } else {
                    info!("User cancelled deletion."); // 사용자가 확인을 누르지 않음
                }
            };
            
            // 컴포넌트 UI 정의
            view! {
                <h1>"Information"</h1> // 페이지 제목

                // 사용자 ID 표시
                <p>{"환영합니다, "}{username_display}{"!"}</p> // username_display Signal 값 표시
                
                // 로그아웃 버튼
                <button on:click=move |ev: MouseEvent| {
                    on_logout(ev);
                }>"로그아웃"</button>

                // 사용자 탈퇴 버튼
                <button on:click=move |ev: MouseEvent| {
                    on_delete_user(ev);
                }>"사용자 탈퇴"</button>
            }.into_view()
        }}
    }
}