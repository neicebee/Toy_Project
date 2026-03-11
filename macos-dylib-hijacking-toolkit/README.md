# macOS Dylib Hijacking Toolkit

macOS 동적 라이브러리(dylib) 하이재킹 취약점을 탐지하고 악용하는 통합 도구입니다.

## 프로젝트 구조

```
macos-dylib-hijacking-toolkit/
├── scanner/src/                # Scanner 모듈 (취약점 탐지)
│   ├── main.c, *.h / *.c
│   ├── Makefile
│   └── README.md               # Scanner 상세 문서
│
├── loader/src/                 # Loader 모듈 (dylib 주입)
│   ├── main.c, *.h / *.c
│   ├── Makefile
│   └── README.md               # Loader 상세 문서
│
├── payload/                    # Payload dylib 템플릿
│   └── template.c              # 기본 dylib 템플릿
│
├── build/                      # 빌드 출력 (바이너리)
│   ├── dylib_auditor           # Scanner 바이너리
│   └── dylib_loader            # Loader 바이너리
│
├── Makefile                    # 프로젝트 통합 빌드 스크립트
└── README.md                   # 이 문서
```

## 모듈 설명

### 🔍 Scanner (`scanner/`)
- **목표**: macOS 시스템의 dylib 하이재킹 취약점 탐지
- **기능**:
  - Mach-O 바이너리 파싱
  - @rpath, @loader_path, @executable_path 기반 의존성 분석
  - 코드 서명 검증
  - Weak dylib 탐지
  - 취약한 로딩 경로 식별
- **출력**: 바이너리별 취약점 리포트

### 🔗 Loader (`loader/`)
- **목표**: 취약한 프로세스에 악의적인 dylib 주입
- **기능**:
  - Scanner 결과 파싱
  - RPATH 취약점 악용
  - Weak Dylib 기반 주입
  - 하이브리드 공격 모드
- **입력**: Scanner에서 생성한 리포트

### 💉 Payload (`payload/`)
- **목표**: 주입 가능한 dylib 템플릿 및 컴파일 기본값 제공
- **파일**: `template.c` - 기본 dylib 템플릿
- **특징**: 런타임 시 동적으로 컴파일되어 바이너리에 주입됨

## 빌드 방법

### 전체 빌드
```bash
make build
```

### 특정 모듈만 빌드
```bash
make scanner    # Scanner만 빌드
make loader     # Loader만 빌드
```

### 정리하기
```bash
make clean      # 모든 빌드 아티팩트 제거
```

### 도움말
```bash
make help       # 사용 가능한 타겟 표시
```

## 빌드 결과

빌드 완료 후 바이너리는 `build/` 디렉토리에 생성됩니다:
- `build/dylib_auditor` - Scanner 바이너리
- `build/dylib_loader` - Loader 바이너리

## 시작하기

1. **프로젝트 빌드**
   ```bash
   make build
   ```

2. **Scanner 실행** - 취약점 탐지 (결과는 `payload/report.txt`에 저장)
   ```bash
   ./build/dylib_auditor
   ```

3. **Loader 실행** - 자동으로 Scanner 결과 파일을 읽고 실행
   ```bash
   ./build/dylib_loader
   ```

### 참고: Scanner 옵션
```bash
# 기본 (결과를 payload/report.txt에 저장)
./build/dylib_auditor

# Verbose 모드
./build/dylib_auditor -v

# 결과를 다른 파일에 저장
./build/dylib_auditor -o /tmp/custom_report.txt

# 결과를 stdout으로 출력
./build/dylib_auditor --stdout
```

## 의존성

- macOS 10.x 이상
- Xcode 또는 clang (C 컴파일러)
- CoreFoundation 프레임워크
- Security 프레임워크

## 상세 문서

각 모듈에 대한 자세한 정보:

- **[Scanner 상세 문서](scanner/README.md)** - 취약점 탐지 알고리즘, 사용법, 예시
- **[Loader 상세 문서](loader/README.md)** - 주입 전략, DYLD 심볼 해석, 상세 구현

## 라이선스

교육 및 연구 목적으로만 사용하세요.

---

**마지막 구조 변경**: 2026년 3월
