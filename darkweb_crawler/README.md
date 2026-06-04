# Darkweb Domain Analyzer

다크웹 `.onion` 도메인을 입력하면 Tor 네트워크를 통해 HTML을 수집하고, DarkBERT 기반 AI 분류기로 범죄 카테고리를 판별하는 분석 도구입니다.

---

## 주요 기능

| 기능 | 설명 |
|------|------|
| HTML 수집 | Tor SOCKS5 프록시를 통해 .onion 사이트 크롤링 |
| CoDA 범죄 분류 | DarkBERT + LogisticRegression으로 범죄 카테고리 분류 (93% 정확도) |
| 사이트 유형 분류 | BART zero-shot 분류기로 포럼·마켓플레이스·블로그 등 유형 판별 |
| LLM 사이트 요약 | OpenAI API (GPT-4o-mini) 기반 사이트 목적·위험도 자동 분석 |
| 검색 색인 확인 | Ahmia, DuckDuckGo 등재 여부 확인 |
| HTML 보고서 생성 | 분석 결과를 시각화한 HTML 보고서 자동 생성 |
| CSAM 안전장치 | 아동 관련 불법 콘텐츠 감지 시 즉시 차단 |

---

## CoDA 범죄 카테고리 분류

### 분류 방식

키워드 매칭이 아닌 **AI 모델이 문맥 전체를 이해해서 판단**합니다.

```
1. HTML 수집
   ↓
2. HTML 정제 (태그/스크립트 제거 → 순수 텍스트)
   ↓
3. DarkBERT 임베딩 (텍스트 → 768차원 벡터)
   ↓
4. LogisticRegression 분류 (벡터 → 카테고리별 확률)
   ↓
5. 결과 출력: Drugs 87%, Hacking 9%, ...
```

**예시**:
- 입력 텍스트: "Buy cocaine online with Bitcoin"
- 벡터 변환: [0.21, -0.45, 0.87, ... (768개)]
- 분류 결과: **Drugs 92%**, Arms 5%, Financial 3%

### DarkBERT란?

S2W(한국 보안 전문 기업)가 **실제 다크웹 텍스트로 사전학습**한 BERT 기반 모델입니다.

**일반 BERT vs DarkBERT**:
- 일반 BERT: Wikipedia, 뉴스 기사로 학습 → 다크웹 언어 이해 부족
- **DarkBERT**: 실제 다크웹 사이트 텍스트로 학습 → `escrow`, `PGP`, `vendor`, `onion` 같은 다크웹 특화 표현 완벽 이해

**임베딩 원리**:
- 입력: "Buy cocaine" → 벡터로 변환
- 의미가 비슷한 문서 → 비슷한 벡터 패턴
- 768차원의 고차원 공간에서 의미 거리를 계산
- 범죄 카테고리별 중심점(centroid) 거리로 분류

### 학습 데이터

| 항목 | 내용 |
|------|------|
| 출처 | S2W CoDA (Cybercrime Ontology for Darkweb Analysis) |
| 수량 | 10,000개 실제 다크웹 사이트 데이터 |
| 허가 | S2W로부터 연구 목적 사용 허가 수령 |
| 전처리 | Others 카테고리 제외 후 7,081개로 학습 |

### 9개 범죄 카테고리 & 정확도

| 카테고리 | 설명 | F1 정확도 | 예시 |
|----------|------|----------|------|
| 🎰 **Gambling** | 도박, 베팅 사이트 | **98%** | 온라인 카지노, 스포츠 베팅 |
| 🔞 **Porn** | 성인/음란 콘텐츠 | **93%** | 불법 성인 사이트 |
| 💊 **Drugs** | 마약 거래 | **92%** | 마약 판매점, 암호화폐 결제 |
| 🔫 **Arms** | 무기 거래 | **92%** | 총기, 폭발물 판매 |
| 💻 **Electronic** | 전자기기 불법 거래 | **91%** | 도난 휴대폰, 해킹된 계정 판매 |
| 🔪 **Violence** | 폭력, 살인 청부 | **91%** | 킬러 고용, 폭력 조직 |
| 💰 **Financial** | 불법 금융, 사기 | **87%** | 위조 지폐, 신용카드 사기 |
| ₿ **Crypto** | 암호화폐 세탁 | **82%** | 믹싱 서비스, 환치 서비스 |
| 🖥️ **Hacking** | 해킹 서비스 | **81%** | 0-day 익스플로잇, DDoS 대행 |

> **전체 정확도 93%** (검증 데이터 1,063개 기준)

### 불확실성 처리

**규칙**: 1위 vs 2위 확률 차이 < 10% → `⚠️ 불확실` 경고 표시

**예시**:
- ✅ `Drugs 87%` vs `Hacking 8%` → 차이 79% → 명확 (Drugs)
- ⚠️ `Drugs 45%` vs `Hacking 38%` → 차이 7% → 불확실 (경고 표시)

**불확실성 발생 원인**:
1. 사이트가 여러 카테고리 콘텐츠 혼합
2. 수집된 HTML이 불충분 (예: 메인페이지만 수집)
3. 모호한 텍스트 표현

→ 보고서에서 명확히 표시하여 사용자 주의 유도

---

## 시스템 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│          웹 브라우저 (http://localhost:8080)             │
└────────────────┬────────────────────────────────────────┘
                 │
         web/app.py (Flask)
                 │
         agent.py (메인 에이전트)
                 │
      ┌──────────┴──────────┐
      │                     │
   로컬 분석             원격 서버
   (로컬 모드)           (분리 모드)
      │                     │
  analyzers/          server/app.py
  reporters/                │
                    Tor SOCKS5
                           │
                    .onion 도메인 크롤링

┌─────────────────── 로컬 분석 모듈 ───────────────────┐
│ • coda_classifier.py    → DarkBERT + LR (93%)        │
│ • category_classifier.py → BART zero-shot 분류      │
│ • llm_analyzer.py       → GPT-4o-mini (선택)         │
│ • agent_report_generator.py → HTML 보고서            │
└──────────────────────────────────────────────────────┘
```

---

## 보고서 구성

생성된 HTML 보고서에는 다음 항목이 포함됩니다.

### 1️⃣ 요약 카드 (상단)
| 정보 | 설명 |
|------|------|
| **AI 위험도** | 1위 카테고리 + 확률 |
| **사이트 유형** | 포럼 / 마켓플레이스 / 블로그 등 |
| **접근성** | HTTP 상태 + HTML 수집 여부 |
| **⚠️ 경고** | HTML 미수집 시 분석 제한 표시 |

### 2️⃣ AI 분석 섹션
| 항목 | 설명 |
|------|------|
| **목적** | 사이트의 실제 목적 분석 (GPT-4o-mini) |
| **요약** | 콘텐츠 요약 및 특징 |
| **위험도** | 보안 위협 수준 평가 |

### 3️⃣ 기술 정보
| 항목 | 설명 |
|------|------|
| **접근성** | HTTP 상태코드, 응답시간, 암호화 여부 |
| **검색 색인** | Ahmia 결과 수, DuckDuckGo 등재 여부 |

### 4️⃣ 범죄, 사이트 유형 카테고리 그래프
- **7개 사이트 유형별 확률**: 막대 그래프
- **9개 카테고리별 확률**: 막대 그래프 (상위부터 정렬)
- **1위/2위 표시**: 명확성 표기
- **⚠️ 불확실성**: 차이 <10% 시 경고

---

## 설치 및 실행

### 구성 방식 선택

**방식 1: 로컬 단독 실행** (추천: 간단)
```
웹 UI (8080) → agent.py → 분석 서버 (5001) → Tor → .onion
모든 컴포넌트가 같은 머신에서 실행
```

**방식 2: 분리 운영** (고급: Ubuntu 서버 별도)
```
로컬 Mac/Windows           Ubuntu 서버
web/app.py (8080)    →    server/app.py (5001)
agent.py             →    Tor 프록시
(분석)                     (크롤링)
```

---

### 로컬 단독 실행 가이드

#### 1️⃣ 의존 패키지 설치

```bash
pip install -r requirements.txt
```

#### 2️⃣ Tor 설치 및 실행

**Windows (WSL2 또는 Docker 권장)**:
```bash
# WSL2 Ubuntu에서
sudo apt update && sudo apt install tor
sudo service tor start
```

**macOS**:
```bash
brew install tor
brew services start tor
```

**Linux**:
```bash
sudo apt update && sudo apt install tor
sudo service tor start
```

**Tor 상태 확인**:
```bash
# SOCKS5 포트 9050 확인
netstat -an | grep 9050  # Linux/macOS
netstat -an | findstr 9050  # Windows
```

#### 3️⃣ 분석 서버 실행

**터미널 1** - 분석 서버 (포트 5001):
```bash
python server/app.py
# 또는
python3 server/app.py
```

**터미널 2** - 웹 UI (포트 8080):
```bash
python web/app.py
# 또는
python3 web/app.py
```

> ⚠️ **중요**: 두 서버가 **동시에 실행 중**이어야 분석이 동작합니다.  
> 종료 시 반드시 `Ctrl+C`를 사용하세요 (창을 닫으면 포트 충돌이 발생합니다).

#### 4️⃣ 브라우저 접속

```
http://localhost:8080
```

---
