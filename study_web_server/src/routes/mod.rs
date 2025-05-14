use actix_web::{web, HttpResponse, Responder};
use serde::Deserialize;
use sqlx::{Row, SqlitePool};

// main.rs에서 App::configure로 호출되어 라우트 설정 담당
pub fn init(cfg: &mut web::ServiceConfig) { // web::ServiceConfig를 가변 참조로 받아 설정 변경
    // "/api/exam" 경로에 대한 라우트 설정
    cfg.service(    // 서비스 등록
        web::resource("/api/exam")  // 리소스 정의
            .route(web::post().to(create_exam)) // 해당 리소스에 post 요청 수신 시 create_exam 함수로 연결
            .route(web::get().to(list_exam))    // 해당 리소스에 get 요청 수신 시 list_exam 함수로 연결
    );
}

// post /api/exam 요청 시 json 본문을 이 구조체로 역직렬화하는데 사용
#[derive(Deserialize)]  // serde 크레이트의 Deserialize 트레이트 자동 구현
struct CreateExam {
    title: String,  // json의 "title" 필드를 String으로 매핑
}

// 핸들러 함수
async fn create_exam(pool: web::Data<SqlitePool>, json: web::Json<CreateExam>) -> impl Responder {
    // web::Json 익스트랙터를 통해 요청 본문의 json 데이터를 CreateExam 구조체로 역직렬화하여 받음
    // web::Data<SqlitePool> 익스트랙터를 통해 main에서 등록한 DB 풀 객체를 받음
    let title = json.title.clone();
    
    // sql insert query 실행
    match sqlx::query("insert into exam (title) values (?)").bind(&title)   // 쿼리 문자열에서 placeholder '?'에 title 값 바인딩(참조 전달)
    .execute(pool.get_ref()).await {    // DB 풀 참조(get_ref())를 사용하여 쿼리 실행 및 완료 대기
        Ok(_) => HttpResponse::Ok().body("Exam created."),  // 성공 시 200 OK 응답
        Err(e) => { // 에러 발생 시
            eprintln!("Error creating exam: {:?}", e);  // 에러 로그 출력(표준 에러)
            HttpResponse::InternalServerError().body("Error creating exam.")    // 500 Internal Server Error 응답
        }
    }
}

#[derive(serde::Serialize)]
struct Exam {   // dp 테이블 매핑되는 구조체
    id: i64,
    title: String,
    created_at: String,
}

// 핸들러 함수
async fn list_exam(pool: web::Data<SqlitePool>) -> impl Responder {
    // web::Data<SqlitePool> 익스트랙터를 통해 main에서 등록한 DB 풀 객체를 받음
    match sqlx::query("select id, title, created_at from exam")
    .fetch_all(pool.get_ref()).await {  // DB 풀 참조를 사용하여 모든 결과 행 가져오기 및 완료 대기(결과 Vec<SqliteRow> 타입)
        Ok(rows) => {   // 성공적으로 결과 행을 가져왔을 때
            let exams: Vec<Exam> = rows.into_iter().map(|r| Exam {  // 각 행(r: SqliteRow)을 Exam 구조체로 매핑
                id: r.get("id"),
                title: r.get("title"),
                created_at: r.get("created_at"),
            }).collect();   // 매핑된 Exam 객체들을 Vec<Exam>으로 수집
            HttpResponse::Ok().json(exams)  // Vec<Exam>을 json 형태로 직렬화하여 200 OK 응답
        }
        Err(e) => { // 에러 발생 시
            eprintln!("Error listing exams: {:?}", e);  // 에러 로그 출력(표준 에러)
            HttpResponse::InternalServerError().body("Error listing exams.")    // 500 Internal Server Error 응답
        }
    }    
}