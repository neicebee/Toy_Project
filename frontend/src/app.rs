use leptos::*;
use leptos_router::*;

// 라우트 핸들러 컴포넌트들을 임포트할 위치
use crate::pages::register::Register;

// Root Component (라우팅 설정 포함)
#[component]
pub fn App() -> impl IntoView {
    view! {
        // 라우터 컴포넌트: 클라이언트 사이드 라우팅 활성화
        <Router>
            // 라우트 정의: 특정 경로에 어떤 컴포넌트를 렌더링할지 매핑
            <Routes>
                // TODO: 나중에 / 경로에 대한 랜딩 페이지 또는 리다이렉트 설정
                // TODO: 나중에 /login 경로에 대한 라우트 설정
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

// TODO: Register 컴포넌트를 src/pages/register.rs 파일에 만들고 여기서 사용하도록 수정할 예정
// src/pages 디렉토리를 만들고 mod.rs 파일 추가
// src/pages/mod.rs: pub mod register;
// src/app.rs: use crate::pages::register::Register;