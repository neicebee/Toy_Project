use serde::{Serialize, Deserialize};
use jsonwebtoken::{encode, decode, Header, Validation, EncodingKey, DecodingKey};
use anyhow::Result;
use std::time::{SystemTime, UNIX_EPOCH};

const SECRET_KEY: &[u8] = b"secret_key_for_jwt";

#[derive(Serialize, Deserialize)]
struct Claims {
    sub: String,
    exp: usize,
}

pub fn create_jwt(username: &str) -> Result<String> {
    let expiration = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs() + 3600;    // 1시간 유효
    let claims = Claims {
        sub: username.to_string(),
        exp: expiration as usize,
    };
    let token = encode(&Header::default(), &claims, &EncodingKey::from_secret(SECRET_KEY))?;
    Ok(token)
}

pub fn decode_jwt(token: &str) -> Result<String> {
    let data = decode::<Claims>(
        token,
        &DecodingKey::from_secret(SECRET_KEY),
        &Validation::default()
    )?;
    Ok(data.claims.sub)
}