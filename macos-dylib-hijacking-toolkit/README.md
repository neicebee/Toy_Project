# macOS Dylib Hijacking Toolkit
### 동적 라이브러리 공급망 공격 재현 및 검증 플랫폼

> **Phase 1 완료**: Type A (RPATH), Type B (Framework), Type C (Modular Chain) 검증 완료  
> **테스트 환경**: macOS Tahoe 26.5.2

## 🎯 프로젝트 개요

본 툴킷은 macOS의 동적 라이브러리 로딩 메커니즘에서 발생하는 공급망 공격 취약점을 **탐지하고 재현하며 검증**하는 연구 플랫폼입니다.

**핵심 기능**:
- ✅ Scanner: Mach-O 바이너리 분석으로 취약점 자동 탐지
- ✅ Loader: 탐지된 취약점을 이용한 실제 공격 재현
- ✅ 3가지 공격 타입 검증 (Type A/B/C)
- ✅ 마커 파일 + 로그 기반 자동 검증

---

## 📊 Phase 1 완성도

| Type | 공격 방식 | 대상 | 검증 결과 | 성공률 |
|------|----------|------|----------|--------|
| **Type A** | RPATH dylib 주입 | IINA | ✅ 성공 | 100% |
| **Type B** | Framework 공급망 | IINA, AltTab, Stats | ✅ 성공 | 33%* |
| **Type C** | 의존성 체인 공격 | Stats (Kit.framework) | ✅ 성공 | 100% |

> *Type B: 코드 서명 enforcement 약한 앱만 성공 (Team ID 검증 우회 미구현)

---

## 🏗️ 프로젝트 구조

```
macos-dylib-hijacking-toolkit/
├── scanner/                    # Phase 1: 취약점 탐지
│   ├── src/
│   │   ├── main.c             # Scanner 진입점
│   │   ├── macho_parser.c      # Mach-O 파싱
│   │   ├── hijack_detector.c   # RPATH/Weak dylib 취약점 탐지
│   │   └── ...
│   ├── Makefile
│   └── README.md              # Scanner 상세 설명
│
├── loader/                      # Phase 1: 공격 검증
│   ├── src/
│   │   ├── main.c             # Loader 진입점 (Type 분류/선택)
│   │   ├── rpath_injector.c    # Type A: RPATH 주입
│   │   ├── weak_dylib_injector.c # Type A: Weak dylib 주입
│   │   ├── framework_supply_chain.c # Type B: Framework 공격
│   │   ├── dependency_chain.c  # Type C: 의존성 추적
│   │   └── ...
│   ├── Makefile
│   └── README.md              # Loader 상세 설명
│
├── payload/                     # 공격 페이로드
│   ├── template.c             # 악성 dylib 템플릿
│   ├── success.html           # 검증 성공 화면
│   └── report.txt             # Scanner 출력
│
├── build/                       # 컴파일된 바이너리
│   ├── dylib_auditor          # Scanner 실행파일
│   └── dylib_loader           # Loader 실행파일
│
├── Apps.md                      # 테스트 앱 목록
├── Makefile                     # 최상위 빌드
└── README.md                    # 이 파일
```

---

## 🚀 빌드 및 실행

### 1️⃣ 빌드

```bash
cd /Users/f1r3_r41n/Desktop/Toy_Project/macos-dylib-hijacking-toolkit

# 전체 프로젝트 빌드
make build

# 또는 개별 빌드
make -C scanner/src
make -C loader/src
```

### 2️⃣ Scanner 실행 (취약점 탐지)

```bash
./build/dylib_auditor -b /Applications/eval-apps > payload/report.txt

# 또는 프로세스 스캔
./build/dylib_auditor
```

### 3️⃣ Loader 실행 (공격 검증)

```bash
./build/dylib_loader

# 대화형 메뉴:
# 1. 스캔 대상 앱 선택 (IINA, Stats 등)
# 2. 취약점 타입 선택 (Type A/B/C)
# 3. 자동 검증 수행
```

---

## 🔍 각 Type의 특징

### Type A: RPATH Dylib 주입
```
취약한 앱 바이너리
  ↓ RPATH에서 탐색
/tmp/malicious_dylib  ← 우리의 악성 dylib
  ↓ 로드됨
악성 코드 실행 (Constructor)
  ↓
마커 파일 생성: /tmp/type_a_success
```

**성공 사례**: IINA.app (libsoxr.0.dylib)
**검증**: 마커 파일 존재 확인

---

### Type B: Framework 공급망 공격
```
Sparkle.framework (공유)
  ↓ 1개 교체
여러 앱에서 로드:
  - IINA.app ✅
  - AltTab ✅
  - Keka ❌ (Team ID 검증)
  - Boop ❌ (Team ID 검증)
```

**성공 사례**: IINA.app + Sparkle.framework
**제약**: 코드 서명 enforcement 약한 앱만 가능

---

### Type C: 의존성 체인 공격
```
Stats.app
├─ Kit.framework (중심, 모두가 의존)
├─ Battery → Kit ✓
├─ Net → Kit ✓
├─ CPU → Kit ✓
└─ ... (모두 Kit에 의존)

Kit 교체 → 모든 의존 Framework 침해
           → 다중 진입점 (Main + Widget) 모두 영향
```

**성공 사례**: Stats.app + Kit.framework
**특징**: Framework 간 의존성 활용

---

## 📈 성과 및 한계

### ✅ 달성한 것
- 3가지 공격 타입 모두 검증 가능
- IINA, AltTab, Stats에서 100% 성공
- 자동 검증 파이프라인 구축

### ⚠️ 미해결 과제 (Phase 2)
- **Team ID 코드 서명 검증 우회** (Boop, Keka 등 강한 enforcement 앱)
- **Gatekeeper 자체 우회 및 악성 패키지 배포** (향후 공급망 공격 확대)
- SIP (System Integrity Protection) 비활성화 필요
- XPC Service Hijacking (미구현)
- DYLD_INSERT_LIBRARIES (SIP 필요)

---

## 📖 상세 설명서

- [Scanner 상세 가이드](scanner/README.md) - 취약점 탐지 원리
- [Loader 상세 가이드](loader/README.md) - 각 Type별 공격 방식

---

## ⚖️ 라이선스 & 면책

본 툴킷은 **교육 및 학술 연구 목적**으로만 사용하세요.
승인받지 않은 시스템 접근은 불법입니다.

---

**마지막 업데이트**: 2026년 7월 30일 (Phase 1 완료)
