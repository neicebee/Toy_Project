use actix_web::{test, App};
use study_web_server::routes::init;
use sqlx::sqlite::SqlitePoolOptions;

#[actix_web::test]
async fn test_create_and_list_exam() {
    // 임시 in-memory DB
    let temp_pool = SqlitePoolOptions::new().connect("sqlite::memory:")
    .await.unwrap();

    sqlx::query("create table exam (id integer primary key autoincrement, title text not null, created_at datetime default current_timestamp);")
        .execute(&temp_pool).await.unwrap();
    
    let app = test::init_service(
        App::new().app_data(actix_web::web::Data::new(temp_pool))
        .configure(init)
    ).await;
    
    let req = test::TestRequest::post().uri("/api/exam")
        .set_json(&serde_json::json!({"title": "Test Exam"})).to_request();
    
    let resp = test::call_service(&app, req).await;
    assert!(resp.status().is_success());
    
    let req = test::TestRequest::get().uri("/api/exam").to_request();
    let resp: serde_json::Value = test::call_and_read_body_json(&app, req).await;
    assert_eq!(resp.as_array().unwrap().len(), 1);
    assert_eq!(resp[0]["title"], "Test Exam");
}