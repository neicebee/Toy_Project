# macOS Dylib Injection Scanner (dylib_auditor)

## 📌 개요

macOS용 **dylib 하이재킹 및 런타임 취약점 스캐너**입니다. 이 모듈은 시스템의 모든 프로세스를 탐사하여 Mach-O 바이너리를 파싱하고, `@rpath`, `@loader_path`, `@executable_path` 기반 의존성을 분석합니다.

### 주요 기능

- **Mach-O 바이너리 파싱**: Load commands, RPATHs, dylib 목록 추출
- **코드 서명 검증**: Apple 서명 여부, Hardened Runtime, Library Validation 확인
- **RPATH 하이재킹 탐지**: 다중 후보 경로로 인한 취약점 식별
- **Weak Dylib 탐지**: LC_LOAD_WEAK_DYLIB 기반 주입 가능성 분석
- **취약 경로 식별**: 존재하지 않거나 SIP 보호 없는 경로 판별
- **자동 필터링**: Apple 서명 바이너리 및 거짓 양성 자동 제외

---

## 🏗️ 모듈 구조

### 핵심 코드 파일 (src/)

| 파일 | 역할 |
|------|------|
| `main.c/h` | 엔트리 포인트, CLI 옵션 파싱 (`-v`, `-q`, `-o`, `--stdout`) |
| `process_scanner.c/h` | 시스템 프로세스 열거 및 대량 스캔 조율 |
| `binary_scanner.c/h` | 단일 바이너리 종합 스캔 및 결과 집계 |
| `macho_parser.c/h` | Mach-O 파일 파싱, Load commands/RPATHs 추출 |
| `code_signing.c/h` | 코드 서명 정보 조회 및 검증 |
| `path_utils.c/h` | 경로 합성, 해석, 존재 여부 검사 |
| `hijack_detector.c/h` | RPATH/Weak Dylib 하이재킹 탐지 |
| `vuln_scanner.c/h` | 취약 경로(미존재, SIP 미보호) 탐지 |
| `process_report.c/h` | 프로세스/바이너리별 리포트 생성 및 출력 |
| `options.c/h` | 전역 옵션(`g_verbose`) 정의 |
| `Makefile` | 빌드 스크립트 |

---

## 🔍 탐지 로직

### 스캔 프로세스 흐름

```
1. 프로세스 열거
   ├─ 시스템의 모든 실행 중인 프로세스 식별
   └─ PID와 경로 수집

2. 바이너리 분석
   ├─ Mach-O 파일 파싱
   ├─ Load commands 추출 (LC_RPATH, LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB)
   └─ 코드 서명 정보 확인

3. 필터링
   ├─ Apple 서명 (CoreServices, System frameworks) → 제외
   ├─ Hardened Runtime + Lib Validation → 제외
   └─ 알려진 거짓 양성 목록 → 제외

4. 취약점 탐지
   ├─ scan_for_hijack_rpath(): RPATH 기반 다중 후보 검사
   ├─ scan_for_hijack_weak(): Weak Dylib 가능성 분석
   └─ scan_for_vulnerable_rpath(): 사용 불가능한 경로 식별

5. 리포트 생성
   └─ 각 바이너리별 발견된 취약점 정리 및 출력
```

### 탐지 알고리즘

#### RPATH 취약점 탐지

```
For each dylib in LC_LOAD_DYLIB:
  For each rpath in LC_RPATH:
    resolved_path = combine_rpath(rpath, dylib_path)
    
    if is_relative_path(resolved_path):
      resolved_path = resolve_loader_executable_path(binary_path, resolved_path)
    
    if file_exists(resolved_path):
      add to found_paths[]

if len(found_paths) >= 2:
  Check if found_paths[0] is Apple-signed
  if NOT Apple-signed:
    REPORT: RPATH Hijacking Vulnerability
```

#### Weak Dylib 취약점 탐지

```
For each dylib in LC_LOAD_WEAK_DYLIB:
  Find all possible locations where this dylib could be loaded
  if multiple locations exist AND can be written:
    REPORT: Weak Dylib Hijacking Vulnerability
```

#### 취약 경로 탐지

```
For each resolved dylib path:
  if NOT file_exists(path):
    if NOT in_dyld_shared_cache(path):
      if NOT SIP_protected(walk_up_directories(path)):
        REPORT: Vulnerable Path (attackable location)
```

---

## 🛠️ 빌드 방법

### 프로젝트 루트에서 빌드

```bash
# 전체 빌드 (권장)
make build

# Scanner만 빌드
make scanner

# 빌드 아티팩트 정리
make clean
```

### 직접 빌드

```bash
cd scanner/src
make clean && make
```

**빌드 결과**: `../../build/dylib_auditor`

---

## ⚙️ 실행 방법

### 기본 사용법

```bash
# 기본 실행 (결과를 payload/report.txt에 저장)
./build/dylib_auditor

# Verbose 모드 (디버그 정보 출력)
./build/dylib_auditor -v

# 결과를 stdout으로 출력
./build/dylib_auditor --stdout

# 커스텀 경로에 저장
./build/dylib_auditor -o /tmp/custom_report.txt

# 도움말 표시
./build/dylib_auditor -h
```

### 권한 필요

```bash
# 모든 프로세스 정보에 접근하려면 sudo 필요
sudo ./build/dylib_auditor
```

---

## 📊 출력 포맷

### 콘솔 출력 예시

```
════════════════════════════════════════════════════════════════════
                   macOS Dylib Injection Scanner                    
                  (Spectre Vulnerability Detection)                 
════════════════════════════════════════════════════════════════════
[*] 프로세스 스캔 시작...

╔════════════════════════════════════════════════════════════════════╗
║                     스캔 결과 요약 리포트                            ║
╚════════════════════════════════════════════════════════════════════╝

[통계]
  총 스캔 프로세스: 510
  정상 프로세스: 500
  이상 감지 프로세스: 10
```

### 리포트 파일 (payload/report.txt)

- 콘솔과 동일한 형식으로 저장되는 파일
- Loader에서 자동으로 파싱하여 처리

---

## 🔧 CLI 옵션 상세

| 옵션 | 설명 | 기본값 |
|------|------|--------|
| `-v` | Verbose 모드 (디버그 로그 활성화) | 비활성 |
| `-q` | Quiet 모드 (기본값) | 활성 |
| `-o <파일>` | 지정된 파일에 결과 저장 | `payload/report.txt` |
| `--stdout` | stdout으로 출력 (파일 저장 안 함) | 미사용 |
| `-h, --help` | 도움말 표시 | - |

---

## 📈 성능 정보

- **스캔 범위**: 시스템의 모든 실행 중인 프로세스 (통상 500+개)
- **평균 스캔 시간**: 약 10-30초 (시스템 부하에 따라 변동)
- **리포트 파일 크기**: 약 200-500 KB
- **메모리 사용**: 약 50-100 MB

---

## 🐛 알려진 제한사항

1. **권한 제약**: 관리자 권한 없이는 모든 프로세스 정보를 얻을 수 없음
2. **보호된 프로세스**: SIP(System Integrity Protection) 보호 프로세스는 스캔 불가
3. **타이밍 이슈**: 스캔 중 프로세스 종료 시 오류 가능성 있음
4. **Apple 바이너리**: 자동 필터링되므로 검토되지 않음

---

## 📄 라이선스 및 주의사항

교육 및 보안 연구 목적으로만 사용하세요. 무단 접근 및 악의적 목적 사용은 법적 책임을 질 수 있습니다.
