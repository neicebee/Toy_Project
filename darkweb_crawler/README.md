# 🔍 어니언 도메인 분석 에이전트 (Darkweb Domain Analysis Agent)

### 개요
단일 **.onion 도메인**을 입력받아 **종합적인 보안 분석**을 수행하는 에이전트입니다.

- ✅ **접근성 검증**: HTTP 상태코드 확인, 응답 시간 측정
- ✅ **색인 확인**: Ahmia, DuckDuckGo를 통한 공개도 검증
- ✅ **HTML 분석**: 서버에서 안전하게 크롤링
- ✅ **콘텐츠 분석**: 불법 콘텐츠 탐지
- ✅ **카테고리 분류**: 타입별 분류 (포럼, 마켓플레이스 등)
- ✅ **신뢰도 점수**: 40 접근성 + 30 색인 + 30 콘텐츠 = 100점
- ✅ **보고서 생성**: 상세 HTML 보고서 자동 생성

---

## 🚀 빠른 시작

### 로컬 단일 도메인 분석

```bash
# 1️⃣ CLI 모드로 실행
python launcher.py
# 옵션: 1 (CLI 에이전트) 선택

# 2️⃣ 도메인 입력
분석할 도메인: example3j7j5twq22c4dcvq.onion

# 3️⃣ 결과 확인
✅ 보고서 생성 완료: analysis_reports/analysis_report_[domain]_[timestamp].html
```

### 웹 브라우저 기반 분석

```bash
# 1️⃣ 웹 모드로 실행
python launcher.py
# 옵션: 2 (웹 서버) 선택

# 2️⃣ 웹 인터페이스 접속
http://localhost:5000

# 3️⃣ 도메인 입력 후 분석 시작
```

---

## 📁 프로젝트 구조

```
darkweb_crawler/
├── agent.py                          # ⭐ 메인 에이전트 (단일 도메인 분석)
├── launcher.py                       # CLI/웹 모드 선택 도구
├── config/
│   └── config.py                     # 설정 파일 + forums.json
├── server/
│   ├── app.py                        # Flask 웹 서버 (우분투)
│   ├── config.py                     # 서버 설정
│   ├── safe_validators.py            # 접근성 검증
│   ├── indexing_validator.py         # 색인 확인 (Ahmia, DuckDuckGo)
│   ├── concealment_validator.py      # 은닉도 검증
│   └── duckduckgo_client.py          # DuckDuckGo 검색 클라이언트
├── analyzers/
│   ├── content_analyzer.py           # 불법 콘텐츠 탐지
│   ├── category_classifier.py        # 사이트 카테고리 분류
│   └── trust_scorer.py               # 신뢰도 계산
├── reporters/
│   └── agent_report_generator.py     # HTML 보고서 생성
├── utils/
│   ├── logger.py                     # 로깅 설정
│   ├── tor_handler.py                # Tor 연결 관리
│   └── forum_classifier.py           # 포럼 분류기
├── database/
│   └── db_manager.py                 # 데이터베이스 관리
├── analysis_reports/                 # 생성된 분석 보고서 저장
├── logs/                             # 로그 파일
└── requirements.txt                  # 의존 패키지
```

---

## 📊 분석 결과

### 보고서 항목

1. **접근성 정보** 
   - HTTP 상태코드 (0=접근불가, 200=정상, 403=필터, 502=게이트웨이 등)
   - 응답 시간 (초 단위)
   - HTML 수집 상태 (수집됨/미수집)

2. **색인 정보**
   - Ahmia 검색 결과 (검색 건수)
   - DuckDuckGo 검색 결과 (True/False)
   
3. **사이트 카테고리** (HTML 수집된 경우만)
   - 주요 카테고리 (신뢰도 포함)
   - 부가 카테고리
   
4. **불법 콘텐츠** (HTML 수집된 경우만)
   - 판정 결과 (합법/불법)
   - 신뢰도 %
   - 탐지 타입 (마약, 무기, 문서 위조 등)
   
5. **신뢰도 분석**
   - 종합 점수 (0-100)
   - 신뢰 수준 (매우 높음/높음/보통/낮음/매우 낮음)
   - 점수 구성 (접근성/색인/콘텐츠 비율)

---

## ⚙️ 설정

### config/config.py

```python
# Tor 설정
TOR_SOCKS5_HOST = '127.0.0.1'
TOR_SOCKS5_PORT = 9050

# DDOS 필터 우회 (Ubuntu 서버)
DDOS_FILTER_RETRY_DELAY = 30          # 초기 대기 시간 (초)
DDOS_FILTER_MAX_RETRIES = 5           # 최대 재시도 횟수
REQUEST_DELAY_BETWEEN_RETRIES = 5     # 재시도 간격 (초)
REQUEST_BETWEEN_DOMAINS = 10          # 도메인 간 대기 시간 (초)
USE_ROTATING_USER_AGENTS = True       # User-Agent 회전
TOR_IDENTITY_CHANGE_ENABLED = True    # Tor 식별성 변경 활성화
TOR_CONTROL_PORT = 9051               # Tor Control Port (선택사항)
```

---

## 🔧 시스템 요구사항

- **Python**: 3.8+
- **Tor**: 로컬 SOCKS5 프록시 (9050 포트) 또는 Ubuntu 서버
- **Tor Control Port**: 선택사항 (9051 - DDOS 우회 성능 향상)
- **OS**: Linux/macOS (Ubuntu 20.04+ 권장)

---

## 📖 상세 가이드

- [원격 서버 설정](REMOTE_SERVER_SETUP.md)
- [Tor 설정](TOR_SETUP_GUIDE.md)
- [빠른 시작 (원격)](QUICKSTART_REMOTE.md)
- [API 스키마](API_SCHEMA.md)

---

## 🎯 주요 기능

| 기능 | 상태 | 설명 |
|------|------|------|
| CLI 분석 | ✅ | 로컬에서 단일 도메인 분석 |
| 웹 인터페이스 | ✅ | 브라우저 기반 분석 |
| Tor 프록시 | ✅ | SOCKS5 기반 익명 접속 |
| DDOS 우회 | ✅ | 자동 재시도 + Tor 식별성 변경 |
| 액세스 큐 | ✅ | Dread 접근 큐 자동 우회 |
| 콘텐츠 분석 | ✅ | 불법 콘텐츠 탐지 |
| 카테고리 분류 | ✅ | AI 기반 사이트 타입 분류 |
| HTML 보고서 | ✅ | 자동 생성 및 차트 포함 |

---

**마지막 업데이트**: 2026년 4월 15일
