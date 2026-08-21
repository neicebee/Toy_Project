# dylib_loader: 동적 라이브러리 공격 검증 플랫폼

`dylib_loader`는 탐지된 dylib 로딩 취약점을 **실제로 악용하고 검증**하는 도구입니다.

---

## 🎯 핵심 기능

3가지 공격 타입을 자동으로 분류하고 검증:

### Type A: RPATH Dylib 주입
**공격 방식**: RPATH 경로에 악성 dylib 배치
- 대상: 단순 RPATH 의존성을 가진 dylib
- 예: IINA.app의 libsoxr.0.dylib
- 성공률: 100% (코드 서명 제약 없음)

```
Step 1: 원본 dylib 백업
Step 2: 악성 dylib 컴파일 (re-export + marker)
Step 3: RPATH 경로에 배치
Step 4: 앱 실행 → 마커 파일 생성 확인
Step 5: 원본 복구
```

---

### Type B: Framework 공급망 공격
**공격 방식**: 공유 Framework 직접 교체
- 대상: 여러 앱이 공유하는 Framework
- 예: Sparkle.framework (IINA, AltTab 공유)
- 성공률: ~33% (코드 서명 enforcement에 따라)

```
Step 1: 원본 Framework dylib 백업
Step 2: Re-export dylib 생성 (원본 + 악성 코드)
Step 3: Framework dylib 교체
Step 4: 앱 실행 → 마커 파일 생성 확인
Step 5: 원본 복구
```

**핵심**: 1개 Framework 교체로 모든 공유 앱 침해 가능

---

### Type C: 의존성 체인 공격
**공격 방식**: 중심 Framework를 통한 다중 침해
- 대상: Framework 간 의존성이 강한 앱
- 예: Stats.app의 Kit.framework
  - Battery → Kit ✓
  - Net → Kit ✓
  - CPU → Kit ✓
  - ... (모두 Kit에 의존)
- 성공률: 100% (내부 Framework)

```
Step 1: 의존성 분석 (Kit이 중심)
Step 2: Kit.framework를 re-export dylib로 교체
Step 3: 모든 의존 Framework 자동 침해
Step 4: 다중 진입점 (Main + Widget) 모두 영향
Step 5: 원본 복구
```

**핵심**: 1개 중심 Framework로 전체 앱 침해

---

## 🚀 사용법

```bash
./build/dylib_loader

# 대화형 메뉴:
# 1. 취약한 앱 선택 (IINA, Stats 등)
# 2. 취약점 타입 자동 분류 표시
# 3. 검증할 Type 선택
# 4. 자동 공격 재현 및 검증
```

---

## 📋 Type 자동 분류 로직

```c
if (fw_count >= 4 || rpath_count >= 8) {
    // Type C: 복잡한 모듈화 구조
} else if (fw_count >= 1 && fw_count <= 3) {
    // Type B: Framework 공급망
} else {
    // Type A: 단순 RPATH
}
```

---

## ✅ 검증 방식

모든 Type이 **마커 파일 + 로그** 기반 검증 사용:

```
1. 마커 파일 경로: /tmp/sc_FrameworkName_success
2. 로그 파일 경로: /tmp/sc_FrameworkName_attack.log
3. 검증: 마커 파일 존재 = 공격 성공
4. 자동 원본 복구
```

---

## 📊 검증 결과

| 타입 | 앱 | 취약점 | 검증 | 성공 |
|------|-----|--------|------|------|
| A | IINA | libsoxr.0.dylib | ✅ | 성공 |
| B | IINA | Sparkle.framework | ✅ | 성공 |
| B | AltTab | Sparkle.framework | ✅ | 성공 |
| B | Boop | SavannaKit.framework | ❌ | 코드 서명 |
| C | Stats | Kit.framework | ✅ | 성공 |

---

## 🔧 주요 모듈

- `rpath_injector.c`: Type A RPATH 주입
- `weak_dylib_injector.c`: Type A Weak dylib 주입
- `framework_supply_chain.c`: Type B/C Re-export 공격
- `dependency_chain.c`: 의존성 그래프 분석

---

## ⚙️ 빌드

```bash
cd loader/src
make
# → ../build/dylib_loader 생성
```

---

**다음 단계**: 
- Phase 2: Team ID 코드 서명 우회 구현
- Phase 2+: Gatekeeper 우회를 통한 악성 패키지 배포 연구

**테스트 환경**: macOS Tahoe 26.5.2
