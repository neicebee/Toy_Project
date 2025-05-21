use serde::{Serialize, Deserialize};
use jsonwebtoken::{encode, decode, Header, Validation, EncodingKey, DecodingKey};
use anyhow::Result;
use std::time::{SystemTime, UNIX_EPOCH};    // 현재 시간 및 Unix epoch 시간 가져오기

// JWT 서명 및 검증에 사용할 비밀 키 정의
const SECRET_KEY: &[u8] = b"secret_key_for_jwt";

#[derive(Serialize, Deserialize)]
// JWT Payroad에 담길 Claim 정보 정의
struct Claims {
    sub: String,    // sub: 토큰 주체(Subject), 사용자 이름 저장용
    exp: usize,     // exp: 토큰 만료 시간(Expiration Time), Unix Timestamp(초)
}

pub fn create_jwt(username: &str) -> Result<String> {
    // 토큰 만료 시간 계산(Unix Timestamp)
    // SystemTime::now(): 현재 시스템 시간
    // duration_since(UNIX_EPOCH): 1970/01/01 00:00:00UTC 이후 경과 시간 계산
    // +3600: 현재 시간 + 3600초(1시간)
    let expiration = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs() + 3600;    // 1시간 유효
    let claims = Claims {
        sub: username.to_string(),  // 사용자 이름 복제 후 String 저장
        exp: expiration as usize,   // 토큰 만료 시간 u64 -> usize 저장
    };
    
    // JWT 토큰 생성
    // 기본 헤더, 생성한 클레임, 정의한 비밀 키를 사용하여 토큰 인코딩
    let token = encode(&Header::default(), &claims, &EncodingKey::from_secret(SECRET_KEY))?;
    Ok(token)
}

pub fn decode_jwt(token: &str) -> Result<String> {  // 검증할 JWT 토큰 문자열 참조 매개변수
    let data = decode::<Claims>(    // JWT 토큰을 Claims 구조체 타입으로 디코딩
        token,  // 디코딩할 토큰 문자열
        &DecodingKey::from_secret(SECRET_KEY),  // 복호화 비밀 키
        &Validation::default()  // 만료 시간 검증
    )?;
    Ok(data.claims.sub) // 디코딩된 데이터에서 클레임의 주체 필드 값 추출 반환
}