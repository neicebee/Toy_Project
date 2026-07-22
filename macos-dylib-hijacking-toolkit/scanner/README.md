# macOS Dylib Injection Scanner (dylib_auditor)

## 📌 개요

macOS용 **dylib 하이재킹 및 런타임 취약점 스캐너**입니다. 이 모듈은 시스템의 모든 프로세스를 탐사하여 Mach-O 바이너리를 파싱하고, `@rpath`, `@loader_path`, `@executable_path` 기반 의존성을 분석합니다.

## 🔍 탐지 기준 및 취약점 분류 체계
본 스캐너는 다중 RPATH 환경의 해석 모델을 기반으로 취약 조건을 다음 3가지로 분류(Scoring)하여 정량화합니다.

- **Type A (다중 RPATH 순회 취약점):** 초기 RPATH가 SIP 보호 경로를 가리키지만 라이브러리가 존재하지 않아, 후속 RPATH 탐색 시 사용자 쓰기 가능 경로(예: `/tmp`, `~/Library/...`)로 폴백(Fallback)되어 라이브러리 치환이 가능한 경우.
- **Type B (직접 매핑 취약점):** RPATH 자체가 직접적으로 사용자 쓰기 가능 경로에 매핑되어 있는 Dylib.
- **Type C (Weak Dylib 취약점):** `LC_LOAD_WEAK_DYLIB`으로 로드되는 라이브러리가 사용자 쓰기 가능 경로에 존재하거나, 탐색 경로 상에서 조작될 수 있는 경우.

## 🛠 주요 동작 로직
1. **Mach-O 파싱 (`macho_parser.c`):** 대상 바이너리의 로드 커맨드(`LC_RPATH`, `LC_LOAD_DYLIB`, `LC_LOAD_WEAK_DYLIB`)를 추출합니다.
2. **경로 조합 및 SIP 검증 (`path_utils.c`, `bundle_scanner.c`):** 다중 RPATH 결합 경로를 생성하고, 파일 시스템 상의 객체 존재 여부 및 SIP(System Integrity Protection) 보호 여부를 판단합니다.
3. **리포트 생성 (`scan_report.c`):** 탐지 결과를 구조화된 형태(Text/JSON/CSV)로 기록합니다.

## ⚙️ 사용법
```bash
./build/auditor [옵션] [대상 바이너리 경로]
```

## 📈 성능 정보

- **스캔 범위**: 시스템의 모든 실행 중인 프로세스 (통상 500+개)
- **평균 스캔 시간**: 약 10-30초 (시스템 부하에 따라 변동)
- **리포트 파일 크기**: 약 200-500 KB
- **메모리 사용**: 약 50-100 MB

---

## 📄 라이선스 및 주의사항

교육 및 보안 연구 목적으로만 사용하세요. 무단 접근 및 악의적 목적 사용은 법적 책임을 질 수 있습니다.