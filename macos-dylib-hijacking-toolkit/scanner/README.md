# dylib_auditor: macOS 취약점 스캐너

`dylib_auditor`는 macOS 바이너리의 **동적 라이브러리 로딩 취약점**을 자동으로 탐지합니다.

---

## 🎯 주요 기능

### 1. RPATH 기반 취약점 탐지
```
이진 파일: @rpath 경로 추출
  ↓
취약성 분석: 경로가 사용자 쓰기 가능한가?
  ↓
결과: RPATH 취약 경로 목록 (Type A 후보)
```

### 2. Weak Dylib 탐지
```
LC_LOAD_WEAK_DYLIB 추출
  ↓
라이브러리 존재 확인
  ↓
결과: 로드 실패 가능한 약한 라이브러리 (Type A 후보)
```

### 3. Framework 분석
```
앱의 Frameworks 디렉토리 스캔
  ↓
각 Framework의 dylib 의존성 분석
  ↓
결과: 공유 가능한 Framework (Type B 후보)
```

---

## 🚀 사용법

### 번들 모드 (앱 분석)
```bash
./build/dylib_auditor -b /Applications/eval-apps > payload/report.txt
```

### 프로세스 모드 (실행 중 프로세스)
```bash
./build/dylib_auditor
```

---

## 📊 출력 형식

```
바이너리: /Applications/eval-apps/IINA.app/Contents/MacOS/IINA
문제 타입: 취약점
문제명: RPATH 취약 경로
상세: /Applications/eval-apps/IINA.app/Contents/Frameworks/libsoxr.0.dylib; ...
```

---

## ⚙️ 빌드

```bash
cd scanner/src
make
```

**다음 단계**: `dylib_loader`로 탐지된 취약점 검증

**테스트 환경**: macOS Tahoe 26.5.2
