use actix_web::{web, HttpResponse, Responder};
use serde::Deserialize;
use sqlx::{Row, SqlitePool};
use anyhow::Result;
use bcrypt::{hash, verify};
use crate::auth::create_jwt;

#[derive(Deserialize)]
struct RegisterInfo {
    username: String,
    password: String,
}

pub async fn register(pool: web::Data<SqlitePool>, info: web::Json<RegisterInfo>) -> impl Responder {
    let hashed = match hash(&info.password, 10) {
        Ok(h) => h,
        Err(_) => return HttpResponse::InternalServerError().body("Error hasing password...")
    };
    
    match sqlx::query("insert into users(username, password_hash) values (?, ?)").bind(&info.username).bind(&hashed)
        .execute(pool.get_ref()).await {
            Ok(_) => HttpResponse::Ok().body("User registered!"),
            Err(e) => {
                eprintln!("{:?}", e);
                HttpResponse::BadRequest().body("Username already exists or DB Error...")
            }
    }
}

#[derive(Deserialize)]
struct LoginInfo {
    username: String,
    password: String,
}

pub async fn login(pool: web::Data<SqlitePool>, info: web::Json<LoginInfo>) -> impl Responder {
    let row = match sqlx::query("select password_hash from users where username=?").bind(&info.username)
        .fetch_one(pool.get_ref()).await {
            Ok(r) => r,
            Err(_) => return HttpResponse::Unauthorized().body("Invalid username or password..."),
    };
    
    if verify(&info.password, (&row).get("password_hash")).unwrap_or(false) {
        match create_jwt(&info.username) {
            Ok(token) => HttpResponse::Ok().json(serde_json::json!({"token": token})),
            Err(_) => HttpResponse::InternalServerError().body("Error creating token..."),
        }
    } else {
        HttpResponse::Unauthorized().body("Invalid username or password...")
    }
}