#![no_main]
mod app;
mod pages;

use leptos::*;
use wasm_bindgen::prelude::*;
use console_error_panic_hook::set_once;

// App Component 임포트
use app::App;

#[wasm_bindgen(start)]  // WebAssembly entry point for Trunk
pub fn main() {
    // initializes logging for panic hook
    set_once();   // 패닉 발생 시 콘솔에 에러 출력
    
    // Mount the main app component to the body of the HTML document
    leptos::mount_to_body(move || {
        view! {
            <App/>
        }
    });
}