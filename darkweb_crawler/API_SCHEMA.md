# 🔌 API 스키마 - 단일 도메인 분석 에이전트 v2.0

**기본 정보**
- **Base URL**: `http://192.168.64.7:5000`
- **프로토콜**: HTTP (내부 네트워크만)
- **콘텐츠 타입**: JSON
- **인증**: IP 기반 (192.168.64.0/24 허용)
- **Tor 프록시**: socks5h://127.0.0.1:9050

---

## 📚 활성 엔드포인트

### 1️⃣ GET /health

**설명**: 원격 서버 상태 확인 (헬스 체크)

**요청**:
```bash
curl http://192.168.64.7:5000/health
```

**응답** (200 OK):
```json
{
  "status": "healthy",
  "timestamp": "2026-04-15T14:23:45.123456"
}
```

---

### 2️⃣ GET /api/status

**설명**: 서버 상세 상태조회 

모든 검증기(accessibility, indexing, concealment, forum_classifier)가 정상 초기화되었는지 확인

**요청**:
```bash
curl http://192.168.64.7:5000/api/status
```

**응답** (200 OK):
```json
{
  "status": "running",
  "timestamp": "2026-04-15T14:23:45.123456",
  "validators_initialized": true
}
```

---

### 3️⃣ POST /api/analyze_domain ⭐ **메인 엔드포인트**

**설명**: 단일 .onion 도메인 종합 분석

**기능**:
- ✅ 접근성 검증 (HTTP 상태코드, 응답시간)
- ✅ 색인 확인 (Ahmia, DuckDuckGo)
- ✅ HTML 콘텐츠 수집 (상태코드 0 제외)
- ✅ 콘텐츠 분석 (불법 콘텐츠 탐지)
- ✅ 카테고리 분류 (포럼/마켓 등)
- ✅ 신뢰도 점수 계산 (40+30+30)

**요청**:
```bash
curl -X POST http://192.168.64.7:5000/api/analyze_domain \
  -H "Content-Type: application/json" \
  -d '{
    "domain": "dreadytofvwu4oa6.onion"
  }'
```

**요청 본문**:
```json
{
  "domain": "dreadytofvwu4oa6.onion"
}
```

**응답** (200 OK):
```json
{
  "success": true,
  "domain": "dreadytofvwu4oa6.onion",
  "timestamp": "2026-04-15T14:23:45.123456",
  "html_collected": true,
  "analysis_result": {
    "accessibility": {
      "is_accessible": true,
      "status_code": 200,
      "response_time": 2.45,
      "is_active": true
    },
    "indexing": {
      "ahmia_indexed": true,
      "ahmia_result_count": 156,
      "duckduckgo_indexed": false
    },
    "server_analysis": {
      "analysis": {
        "primary_category": "darknet_forum",
        "category_confidence": 0.923
      }
    },
    "content_analysis": {
      "is_illegal": false,
      "illegal_confidence": 0.0,
      "illegal_types": [],
      "primary_illegal_type": null
    },
    "category_result": {
      "primary_category": "darknet_forum",
      "primary_confidence": 0.923,
      "confidence": 0.923
    },
    "trust_score": 78,
    "trust_level": "moderate_trust"
  }
}
```

**응답 필드 설명**:

| 필드 | 설명 | 값 예시 |
|------|------|--------|
| `html_collected` | HTML 수집 여부 (False면 상태코드=0) | true/false |
| `accessibility.status_code` | HTTP 상태코드 (0=응답없음) | 0, 200, 403, 502 |
| `accessibility.response_time` | 응답 시간 (초) | 2.45 |
| `indexing.ahmia_indexed` | Ahmia 색인 여부 | true/false |
| `indexing.ahmia_result_count` | Ahmia 검색 결과 건수 | 0-999+ |
| `content_analysis.is_illegal` | 불법 콘텐츠 포함 여부 | true/false |
| `content_analysis.primary_illegal_type` | 주요 불법 타입 | "drugs", "weapons", "fraud", null |
| `trust_score` | 최종 신뢰도 (0-100) | 78 |
| `trust_level` | 신뢰 수준 | high_risk/moderate_risk/unknown/moderate_trust/high_trust |

**에러 응답** (400 Bad Request):
```json
{
  "success": false,
  "error": "Invalid domain format",
  "message": "Domain must end with .onion"
}
```

**에러 응답** (500 Internal Server Error):
```json
{
  "success": false,
  "error": "Internal server error",
  "message": "Connection timeout or service unavailable"
}
```

---

### 4️⃣ POST /api/validate

**설명**: 도메인 배치 검증 (레거시 - 현재 미사용)

> ⚠️ 배치 처리가 필요한 특수한 경우만 사용
> 새로운 분석은 `/api/analyze_domain` 사용

**요청**:
```bash
curl -X POST http://192.168.64.7:5000/api/validate \
  -H "Content-Type: application/json" \
  -d '{
    "domains": ["domain1.onion", "domain2.onion"],
    "checks": ["accessibility", "indexing", "concealment"]
  }'
```

**응답** (200 OK):
```json
{
  "success": true,
  "timestamp": "2026-04-15T14:23:45.123456",
  "total_domains": 2,
  "completed_domains": 2,
  "results": [
    {
      "domain": "dreadytofvwu4oa6.onion",
      "validity": "valid",
      "checks": {
        "accessibility": {
          "is_accessible": true,
          "status_code": 200,
          "response_time": 2.45
        },
        "indexing": {
          "is_indexed": true,
          "result_count": 156,
          "extracted_domain": null
        },
        "concealment": {
          "is_concealed": false,
          "is_malicious": false,
          "is_blocked": false
        }
      }
    }
  ]
}
```

---

### 5️⃣ POST /api/classify_forum

**설명**: 도메인 포럼 분류 (레거시 - 현재 미사용)

> ⚠️ 레거시 엔드포인트
> 포럼 분류는 `/api/analyze_domain` 응답에 포함됨

---

## 🟲 HTTP 상태 코드

| 코드 | 의미 | 설명 |
|-----|------|------|
| **200** | OK | 요청 성공, 분석 완료 또는 서버 정상 |
| **400** | Bad Request | 잘못된 도메인 형식 또는 필수 파라미터 누락 |
| **403** | Forbidden | 허용되지 않은 IP 주소에서 접속 |
| **500** | Internal Error | Tor 연결 실패, 네트워크 오류 등 |
| **503** | Service Unavailable | 서버 검증기 미초기화 |

---

## 🔐 신뢰도 계산 방식

신뢰도 = 접근성(40점) + 색인(30점) + 콘텐츠(30점)

### 접근성 (40점)
- ✅ 접근 가능 (200): 40점
- ⚠️ 일부 접근 가능 (3xx/4xx): 20점
- ❌ 접근 불가 (5xx/0): 0점

### 색인 (30점)
- ✅ Ahmia + DuckDuckGo 모두 색인: 30점
- ⚠️ 한쪽만 색인: 15점
- ❌ 모두 미색인: 0점

### 콘텐츠 (30점)
- ✅ 불법 콘텐츠 없음: 30점
- ⚠️ 불법 의심 (신뢰도 <50%): 15점
- ❌ 불법 콘텐츠 포함: 0점
- ❓ HTML 미수집: 0점

### 신뢰 수준 (최종 판정)
- **high_risk** (0-20): 위험도 매우 높음
- **moderate_risk** (21-40): 위험도 있음
- **unknown** (41-60): 판단 불가
- **moderate_trust** (61-80): 어느정도 신뢰
- **high_trust** (81-100): 신뢰 높음

---

## 📝 주요 필드 설명

### Domain String 형식
```
일반형태:  "example.onion"
하위도메인: "subdomain.example.onion"
예시: "dreadytofvwu4oa6.onion"
```

### Category Codes (주요 카테고리)
- `darknet_forum`: 다크넷 포럼
- `marketplace`: 마켓플레이스
- `information_portal`: 정보 포털
- `leak_site`: 유출 데이터 공유
- `service`: 기술 서비스
- `social`: SNS/채팅
- `blog`: 블로그/뉴스
- `documentation`: 가이드/문서
- `other`: 기타

### Illegal Types (불법 콘텐츠 분류)
- `drugs`: 약물 거래
- `weapons`: 무기 거래
- `fraud`: 사기/신분증
- `hacking`: 해킹 도구
- `exploit`: 보안 취약점
- `counterfeit`: 위조품
- `stolen_data`: 도난 데이터

---

## 🚀 클라이언트 예제

### Python
```python
import requests

API_BASE = "http://192.168.64.7:5000"

# 도메인 분석
response = requests.post(
    f"{API_BASE}/api/analyze_domain",
    json={"domain": "dreadytofvwu4oa6.onion"},
    headers={"Content-Type": "application/json"}
)

result = response.json()
print(f"신뢰도: {result['analysis_result']['trust_score']}")
print(f"카테고리: {result['analysis_result']['category_result']['primary_category']}")
```

### cURL
```bash
# 배치 검증
curl -X POST http://192.168.64.7:5000/api/validate \
  -H "Content-Type: application/json" \
  -d '{
    "domains": ["dreadytofvwu4oa6.onion"],
    "checks": ["accessibility", "indexing", "concealment"]
  }' | jq .

# 포럼 분류
curl -X POST http://192.168.64.7:5000/api/classify_forum \
  -H "Content-Type: application/json" \
  -d '{"domains": ["dreadytofvwu4oa6.onion"]}' | jq .
```

---

## ⚙️ 설정

### 클라이언트 IP 필터링
```python
ALLOWED_CLIENTS = ["192.168.64.0/24"]  # 구성 파일 참조
```

### Tor 설정
```python
TOR_SOCKS5_HOST = "127.0.0.1"
TOR_SOCKS5_PORT = 9050
TOR_CONTROL_PORT = 9051  # 식별성 변경용
```

### 요청 시간 제한
```python
REQUEST_TIMEOUT = 40  # 초
REQUEST_DELAY_BETWEEN_RETRIES = 3
DDOS_FILTER_MAX_RETRIES = 5
```

---

**마지막 업데이트**: 2026년 4월 15일
