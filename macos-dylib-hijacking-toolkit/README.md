# macOS Dylib Hijacking 점검 통합 툴킷 (Toolkit)

본 툴킷은 macOS 환경에서 다중 RPATH 기반 및 Weak Dylib을 악용한 Dylib Hijacking 취약점을 자동으로 스캔하고 재현 및 검증하는 통합 프레임워크입니다. 
이 도구는 개발 및 배포 전 단계에서 취약한 로딩 조건을 사전에 식별하고 완화(Mitigation)할 수 있도록 설계되었습니다.

## 🌟 주요 기능
- **다중 RPATH 탐색 구조화:** 단일 RPATH뿐만 아니라 후속 RPATH 탐색 중 발생하는 비보호 경로(SIP 미적용 경로, 사용자 쓰기 가능 경로) 노출을 정량적으로 식별합니다.
- **Weak Dylib 검사:** 선택적 의존성(Optional Dependency)을 악용한 하이재킹 가능성을 스캔합니다.
- **통합 점검 파이프라인 (Scanner -> Report -> Loader):** 취약점 탐지에서 멈추지 않고, 프록시(Proxy) Dylib을 임시 경로에 자동 배치하여 실제 악용 가능성(재현 성공률)을 검증합니다.
- **CI/CD 파이프라인 연동:** JSON/CSV 기반의 리포트를 생성하여 Xcode 빌드 후 스크립트 훅 등으로 자동화된 데브옵스(DevOps) 파이프라인에 통합할 수 있습니다.

## 📂 디렉토리 구조
- `/scanner`: Mach-O 바이너리 분석 및 취약 RPATH/Weak Dylib 탐지 모듈 (`dylib_auditor`)
- `/loader`: 탐지된 취약점을 바탕으로 프록시 Dylib을 구성하고 프로세스에 주입하여 검증하는 모듈 (`dylib_loader`)
- `/payload`: 실행 검증 성공 시 동작을 확인할 수 있는 최소한의 페이로드 및 템플릿
- `/Apps.md`: 본 툴킷의 성능(탐지율, 정확도 등)을 정량 평가하기 위해 선정된 오픈소스 및 상용 앱 목록

## 🚀 빌드 및 실행
```bash
# 전체 프로젝트 빌드
make build

# 스캐너 실행 (예시)
./build/auditor --target /Applications/VulnerableApp.app/Contents/MacOS/VulnerableApp

# 로더를 통한 재현 검증 (예시)
./build/loader --report payload/report.txt
```

## 라이선스

교육 및 연구 목적으로만 사용하세요.

---

**마지막 구조 변경**: 2026년 7월