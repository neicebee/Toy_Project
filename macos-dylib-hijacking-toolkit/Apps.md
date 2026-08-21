# Phase 1 검증 대상 애플리케이션

본 논문에서 검증한 **5개 대표 애플리케이션**입니다.

---

## 🎯 검증 앱 목록

### Type A: RPATH 기반 Dylib 주입

| 앱 이름 | 버전 | 프레임워크 | 검증 결과 | 비고 |
|--------|------|-----------|---------|------|
| **4K Video Downloader+** | Latest | Swift + Objective-C | ✅ 성공 | RPATH dylib 단순 구조 |

**특징**:
- 외부 dylib을 RPATH로 동적 로드
- 코드 서명 enforcement 약함
- 마커 파일 생성으로 공격 검증

---

### Type B: Framework 공급망 공격

| 앱 이름 | 공유 Framework | 코드 서명 | 검증 결과 | 비고 |
|--------|---------------|---------|---------|------|
| **IINA** | Sparkle.framework | Weak | ✅ 성공 | 60개 dylib + Sparkle |
| **AltTab** | Sparkle.framework | Weak | ✅ 성공 | 윈도우 관리자 + Sparkle |

**특징**:
- Sparkle.framework 공유 의존성
- 1개 Framework 교체로 다중 앱 침해 가능
- Re-export dylib 기법 사용

**Team ID 검증 문제**:
```
성공한 앱: IINA (코드 서명 enforcement 약함)
성공한 앱: AltTab (코드 서명 enforcement 약함)
실패한 앱: Keka (Team ID 엄격 검증) ❌
실패한 앱: Rectangle (Team ID 엄격 검증) ❌
실패한 앱: Maccy (Team ID 엄격 검증) ❌
실패한 앱: Sloth (Team ID 엄격 검증) ❌
실패한 앱: Ghostty (Team ID 엄격 검증) ❌
실패한 앱: MonitorControl (Team ID 엄격 검증) ❌
```

---

### Type C: Framework 의존성 체인 공격

| 앱 이름 | 중심 Framework | 의존 Framework 수 | 검증 결과 | 비고 |
|--------|---------------|-----------------|---------|------|
| **Stats** | Kit.framework | 10개 | ✅ 성공 | 내부 모듈화 구조 |

**Framework 의존성 구조**:
```
Kit.framework (중심)
├─ Battery → Kit ✓
├─ Net → Kit ✓
├─ CPU → Kit ✓
├─ GPU → Kit ✓
├─ RAM → Kit ✓
├─ Disk → Kit ✓
├─ Clock → Kit ✓
├─ Remote → Kit ✓
├─ Sensors → Kit ✓
└─ Bluetooth → Kit ✓

다중 진입점:
- Main: Stats.app
- Widget: WidgetsExtension.appex
```

**특징**:
- 모든 Framework가 중심 Framework에 의존
- 1개 Framework 교체로 전체 앱 침해
- 다중 진입점 모두 영향

---

### 검증 불가: Team ID 엄격 검증

| 앱 이름 | Framework | 문제점 | 상태 |
|--------|----------|--------|------|
| **Keka** | Sparkle.framework | Team ID 불일치 | ❌ 실패 |
| **Boop** | SavannaKit.framework | Team ID 불일치 | ❌ 실패 |

**실패 원인**:
```
dyld 코드 서명 검증:
  원본 dylib Team ID: Ivan Mathy (RLZ8XBTX7G)
  우리 dylib Team ID: ad-hoc (없음) ✗
  → dyld 로드 거부
```

**Phase 2 해결 필요**:
- Team ID 코드 서명 우회 구현
- SIP 비활성화 환경 대응
- XPC Service Hijacking

---

## 📊 검증 결과 요약

| Type | 성공 | 실패 | 성공률 | 주요 학습 |
|------|------|------|--------|----------|
| **A** | 1 | 0 | 100% | RPATH 주입은 항상 가능 |
| **B** | 2 | 8 | 20% | 코드 서명이 주요 제약 |
| **C** | 1 | 0 | 100% | 내부 Framework은 검증 용이 |
| **합계** | **4** | **8** | **33%** | Phase 2에서 코드 서명 해결 필요 |

---

## 🔍 논문에 포함된 내용

### 성공 사례 (상세 분석)
1. **Type A - 4K Video Downloader+**: RPATH 기본 공격
2. **Type B - IINA + Sparkle**: Framework 공급망 공격
3. **Type C - Stats + Kit**: 의존성 체인 공격

### 실패 사례 분석 (기술적 깊이)
- **Keka vs IINA**: 코드 서명 강도 비교
- **Team ID 검증** 메커니즘 분석
- **dyld 보안** 모델 한계

### 기여도
- macOS dylib 로딩 보안의 현실적 문제 규명
- 3가지 공격 타입 분류 및 검증 방법론
- Phase 2 방향성 제시 (Team ID bypass)

---

**Phase 1 검증 완료**: 2026년 7월 30일  
**테스트 환경**: macOS Tahoe 26.5.2  
**다음 단계**: Phase 2 (코드 서명 우회 + Gatekeeper 우회) - 미구현