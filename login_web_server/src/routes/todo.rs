use actix_web::{web, HttpMessage, HttpResponse, Responder};
use sqlx::{SqlitePool, Row};
use serde::Serialize;
 
#[derive(Serialize)]
struct Todo {
    id: i64,
    title: String,
    created_at: String,
}
 
pub async fn list_todos(pool: web::Data<SqlitePool>, req: actix_web::HttpRequest) -> impl Responder {
    // username 추출
    let _username = req.extensions_mut().get::<String>().cloned().unwrap_or("Unknown".to_string());
 
    // web::Data<SqlitePool> 익스트랙터를 통해 main에서 등록한 DB 풀 객체를 받음
    match sqlx::query("select id, title, created_at from todos")
    .fetch_all(pool.get_ref()).await {  // DB 풀 참조를 사용하여 모든 결과 행 가져오기 및 완료 대기(결과 Vec<SqliteRow> 타입)
        Ok(rows) => {   // 성공적으로 결과 행을 가져왔을 때
            let todos: Vec<Todo> = rows.into_iter().map(|r| Todo {  // 각 행(r: SqliteRow)을 Todo 구조체로 매핑
                id: r.get("id"),
                title: r.get("title"),
                created_at: r.get("created_at"),
            }).collect();   // 매핑된 Todo 객체들을 Vec<Todo>으로 수집
            HttpResponse::Ok().json(todos)  // Vec<Todo>을 json 형태로 직렬화하여 200 OK 응답
        }
        Err(e) => { // 에러 발생 시
            eprintln!("Error listing exams: {:?}", e);  // 에러 로그 출력(표준 에러)
            HttpResponse::InternalServerError().body("Error listing exams.")    // 500 Internal Server Error 응답
        }
    }   
}