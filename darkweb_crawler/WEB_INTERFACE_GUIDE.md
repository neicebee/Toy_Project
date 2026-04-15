# 🌐 웹 인터페이스 도입 - 사용 가이드

Darkweb Crawler가 웹 기반 UI로 크게 개선되었습니다!

## 🚀 빠른 시작

### 웹 모드 (권장)

```bash
# 1. 프로젝트 디렉토리로 이동
cd /Users/f1r3_r41n/Desktop/Toy_Project/darkweb_crawler

# 2. 웹 서버 시작
python launcher.py 2
# 또는 직접 실행: python web/app.py

# 3. 브라우저 열기
# http://localhost:5000
```

### CLI 모드 (기존 방식)

```bash
# 단일 도메인 분석
python launcher.py 1 example.onion

# 또는 직접 실행
python agent.py -d example.onion
```

## 📋 새로운 기능

### 입력 페이지
- ✅ 깔끔한 UI로 도메인 입력
- ✅ 최근 분석 기록 표시 (로컬 저장)
- ✅ 입력값 자동 검증 (.onion 도메인만)

### 분석 진행 중
- ✅ 진행 상황 표시 (1-3분 소요)
- ✅ 실시간 로딩 애니메이션

### 결과 페이지
- ✅ 전체 분석 보고서를 웹에서 직접 보기
- ✅ HTML 보고서 다운로드
- ✅ 반응형 디자인 (모바일 지원)

### 분석 정보
- 📊 **신뢰도 점수**: 0-100점 등급 시스템
- 🌍 **접근성**: 도메인 직접 접근 가능 여부
- 🔍 **색인 정보**: Ahmia, DuckDuckGo 검색 결과
- 📁 **상대 경로**: 접근 가능한 URL 목록
- 🏷️ **카테고리**: 자동 분류 (11가지)
- ⚠️ **콘텐츠 분석**: 불법 콘텐츠 탐지

## 🏗️ 프로젝트 구조

```
darkweb_crawler/
├── launcher.py              # 🆕 시작 스크립트
├── web/                     # 🆕 웹 인터페이스
│   ├── app.py              # Flask 앱
│   ├── README.md           # 웹 모드 설명서
│   ├── templates/
│   │   ├── index.html      # 입력 페이지
│   │   └── result.html     # 결과 페이지
│   └── static/             # CSS, JS (확장용)
├── agent.py                # 기존 CLI 에이전트
├── server/
├── analyzers/
├── reporters/
└── ...
```

## 🎯 사용 시나리오

### 시나리오 1: 웹에서 여러 도메인 순차 분석

1. `python launcher.py 2` 실행
2. http://localhost:5000 접속
3. 첫 번째 도메인 입력 → 분석 완료 → 보고서 확인
4. 이전 페이지로 돌아가 다음 도메인 입력
5. 최근 분석 기록에서 빠른 재분석 가능

### 시나리오 2: 빠른 CLI 분석

1. `python launcher.py 1 example.onion` 실행
2. 콘솔에서 진행 상황 확인
3. 완료 후 `analysis_reports/` 폴더에서 HTML 보고서 확인

### 시나리오 3: 보고서 공유

1. 웹에서 분석 완료
2. "보고서 다운로드" 버튼 클릭
3. HTML 파일을 메일/메신저로 공유
4. 받는 사람이 브라우저에서 직접 열어서 확인

## 🔄 전체 데이터 흐름

```
웹 UI (index.html)
    ↓ (도메인 입력)
    ↓
Flask 앱 (/api/analyze)
    ↓
agent.py (분석 로직)
    ↓
분석 결과 저장 (analysis_reports/)
    ↓
Flask 앱 (/api/report/<domain>)
    ↓
웹 UI (result.html)
    ↓ (iframe에서 표시)
사용자
```

## 🛠️ 기술 스택

| 계층 | 기술 |
|------|------|
| 프론트엔드 | HTML5, CSS3, Vanilla JavaScript |
| 백엔드 | Python Flask |
| 분석 엔진 | 기존 agent.py (변경 없음) |
| 데이터 저장 | HTML 파일 + 로컬 스토리지 |
| 네트워크 | Tor SOCKS5 (기존과 동일) |

## 💾 데이터 저장 위치

| 항목 | 위치 |
|------|------|
| 분석 보고서 | `analysis_reports/` (HTML) |
| 최근 분석 기록 | 브라우저 localStorage |
| 로그 | `logs/` (기존과 동일) |

## 🔐 보안

- ✅ 로컬호스트(127.0.0.1)에서만 실행
- ✅ 포트 5000 (개발용)
- ✅ Tor를 통한 익명 연결 (기존과 동일)
- ⚠️ HTTPS 미지원 (개발 목적)

프로덕션 배포 시:
- HTTPS 설정 필요
- 서버 검증 추가
- 접근 제어 설정

## 📊 성능 특성

| 항목 | 예상치 |
|------|--------|
| 첫 분석 | 2-3분 (토큰 초기화) |
| 이후 분석 | 1-2분 (캐시된 토큰) |
| 보고서 로드 | <1초 |
| 페이지 응답 | <500ms |

## 🐛 문제 해결

### Q: 웹 서버가 실행되지 않음
**A:** Flask 설치 확인
```bash
pip install flask
# 또는 requirements.txt로 설치
pip install -r requirements.txt
```

### Q: localhost:5000에 접속 불가
**A:** 포트 확인
```bash
# 포트 이미 사용 중인지 확인
lsof -i :5000
# 필요시 다른 포트에서 실행
# web/app.py 수정: app.run(port=5001)
```

### Q: 분석 후 보고서가 안 보임
**A:** 보고서 파일 확인
```bash
# 최신 보고서 확인
ls -lt analysis_reports/ | head -5
```

### Q: 분석이 멈춤
**A:** 로그 확인
```bash
# 콘솔의 오류 메시지 확인
# 또는 logs/ 폴더의 로그 파일 확인
```

## 📚 API 엔드포인트

| 엔드포인트 | 메서드 | 설명 |
|-----------|--------|------|
| `/` | GET | 메인 페이지 |
| `/api/analyze` | POST | 도메인 분석 요청 |
| `/api/report/<domain>` | GET | 분석 보고서 조회 |
| `/results` | GET | 결과 페이지 |
| `/health` | GET | 헬스 체크 |

## 🎨 UI 커스터마이징

템플릿 수정:
```bash
# 입력 페이지
web/templates/index.html

# 결과 페이지
web/templates/result.html
```

정적 파일 추가:
```bash
# CSS 추가
web/static/style.css

# JavaScript 추가
web/static/app.js

# 템플릿에서 참조
<link rel="stylesheet" href="/static/style.css">
<script src="/static/app.js"></script>
```

## 📝 변경 사항 요약

### 추가된 파일
- ✅ `launcher.py` - 통합 시작 스크립트
- ✅ `web/app.py` - Flask 웹 서버
- ✅ `web/templates/index.html` - 입력 페이지
- ✅ `web/templates/result.html` - 결과 페이지
- ✅ `web/README.md` - 웹 모드 설명서

### 수정된 파일
- ❌ 없음 (기존 agent.py, analyzer 등 유지)

### 호환성
- ✅ 완전히 호환 (CLI 모드 여전히 작동)
- ✅ 기존 분석 로직 100% 동일

## 🚀 다음 단계

### 현재 상태
- ✅ 웹 인터페이스 완성
- ✅ 도메인 입력/분석/결과 표시
- ✅ 보고서 다운로드

### 향후 개선 사항 (선택)
- API 키 기반 인증
- 다중 사용자 지원
- 분석 큐 시스템
- 실시간 진행률 표시
- 데이터베이스 연동
- 차트/그래프 강화

## 💬 피드백

웹 인터페이스 사용 중 발견된 버그나 개선 사항:
1. 로그 확인: `logs/` 디렉토리
2. console.log 확인: 브라우저 개발자 도구 (F12)
3. Flask 디버그 모드 확인: 콘솔 출력 메시지

---

**행운을 빕니다! 🌟**

질문이나 문제가 있으면 로그와 에러 메시지를 확인해주세요.
