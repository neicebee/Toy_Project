1. 서론
1-1. 배경
macOS의 동적 라이브러리 로딩 메커니즘은 실행 시점의 유연성을 제공하기 위해 정교한 경로 탐색 알고리즘을 사용한다. 특히 RPATH(Runpath Search Path)는 개발자가 상대 경로 기반 의존성을 정의할 수 있도록 설계되어 빌드 프로세스의 복잡성을 크게 낮춘다. 하지만 이러한 유연성은 동시에 공격 표면을 제공한다. 로컬 권한을 가진 공격자는 RPATH가 참조하는 경로에 악의적인 dylib을 배치하여 합법적인 애플리케이션 실행 시 임의 코드를 주입할 수 있다. 더 심각한 문제는 이러한 공격 벡터가 단순한 dylib 치환에 머물지 않는다는 점이다. 다수의 macOS 애플리케이션이 공통의 Framework를 공유할 경우, 한 번의 Framework 교체가 여러 앱에 동시에 영향을 미칠 가능성이 존재한다. 더욱이 내부적으로 모듈화된 구조를 가진 앱 번들에서 중심 Framework가 탈취되면 이를 의존하는 모든 하위 Framework의 보안이 침해된다. 이는 사실상 단일 공급망 진입점을 통한 다중 애플리케이션 침해를 의미한다. 그럼에도 불구하고 macOS Dylib Hijacking은 학술 문헌에서 거의 다루어지지 않았다. Windows의 DLL Hijacking이나 Linux의 LD_PRELOAD 공격에 비해, macOS 특화 연구는 산발적 보안 리포트 수준에 머물러 있다. 이는 학술 연구의 공백이자, 동시에 실무 보안팀의 blind spot을 의미한다.

1-2. 관련 연구
가장 중요한 선행 연구는 Patrick Wardle의 "Dylib Hijacking on OS X"(2015)이다[1]. CanSecWest 2015에서 발표된 이 논문은 macOS가 Windows의 DLL Hijacking과 본질적으로 유사한 동적 라이브러리 로딩 취약점에 노출되어 있음을 최초로 체계적으로 입증했다. Wardle은 weak linking(LC_LOAD_WEAK_DYLIB)과 run-path 의존성의 취약성을 구체적으로 분석하고, 이를 악용한 "Dylib Hijack Scanner" 도구를 공개했다. 이 연구는 이후 모든 dylib hijacking 논의의 기초가 되었다.
- 1: Patrick Wardle, "Dylib Hijacking on OS X," Virus Bulletin Magazine, 2015(3), Mar. 2015.

Windows 환경의 DLL Hijacking은 더욱 광범위하게 연구되었으며, macOS의 동일한 취약점과 본질적 원인을 공유한다. Ge, Payer, Jaeger의 "An Evil Copy: How the Loader Betrays You"(NDSS 2017)는 동적 로더의 근본적 설계 문제를 지적한다[2]. 이들은 Ubuntu 16.04 LTS의 54,045개 패키지 중 4,570개가 예상치 못한 copy relocation을 가지고 있으며, 이것이 memory protection 정보 손실을 초래할 수 있음을 보였다. Check Point Research의 "10 Years of DLL Hijacking, and What We Can Do to Prevent 10 More"(2024)는 수십 개의 실제 DLL Hijacking 캠페인을 분류하고 MITRE ATT&CK의 "DLL Sideloading"으로 정식화했다[3]. 이들 연구는 동적 링커의 순차적 경로 탐색 메커니즘이 모든 주요 OS(Windows, macOS, Linux)에서 공통된 근본 취약점임을 시사한다.
- 2: W. Ge, P. Payer, and R. Jaeger, "An Evil Copy: How the Loader Betrays You," Proceedings of the 24th Network and Distributed System Security Symposium (NDSS), Feb. 2017.
- 3: Check Point Research, "10 Years of DLL Hijacking, and What We Can Do to Prevent 10 More," Security Research, 2024.

Linux 환경에서도 동일한 공격이 문서화되어 있다. 최근 연구 "Resolving the Correct Library: A Loader-Level Defense Solution Against Shared Object Hijacking"(arXiv:2605.26665, 2026)은 glibc의 동적 링커 수준 방어 메커니즘을 제안한다[4]. 이 연구는 공격자가 LD_PRELOAD 환경 변수를 통해 임의의 공유 라이브러리를 먼저 로드할 수 있는 취약성을 분석하고, 빌드 식별자와 암호학적 해시 기반의 검증 방식을 제안한다. SentinelLabs의 "Leveraging LD_AUDIT to Beat the Traditional Linux Library Preloading Technique"는 LD_PRELOAD 우회 기법을 문서화했으며[5], 실제 Jynx2 rootkit은 /etc/ld.so.preload를 악용해 모든 동적 링크 프로세스에 대한 지속적 compromise를 달성했다.
- 4: R. Kernel et al., "Resolving the Correct Library: A Loader-Level Defense Solution Against Shared Object Hijacking," arXiv Preprint arXiv:2605.26665, 2026.
- 5: SentinelLabs, "Leveraging LD_AUDIT to Beat the Traditional Linux Library Preloading Technique," Security Research Blog, 2024.

macOS의 진화된 보안 아키텍처는 이러한 공격을 부분적으로 완화하려 시도한다. WWDC 2023 Session 10061 "Verify app dependencies with digital signatures"는 Xcode 15의 바이너리 Framework 서명 검증을 소개하며, 이는 Framework 공급망 공격을 대응하는 새로운 방어 메커니즘이다[6]. AMFI(Apple Mobile File Integrity)는 모든 dylib 로드 시 code signature를 검증하며, Library Validation(Hardened Runtime 기능)은 같은 Team ID 또는 Apple 서명 dylib만 로드를 허용한다[7][8]. 그러나 이러한 메커니즘의 실제 enforcement 강도는 앱마다 상이하다는 점이 본 연구의 핵심 질문을 제기한다. ad-hoc 서명을 가진 약한 enforcement 앱에 대해서는 여전히 공격이 가능하며, Team ID 검증의 실제 효과는 정량화되지 않았다.
- 6: Apple Inc., "Verify app dependencies with digital signatures," WWDC 2023 Session 10061, June 2023.
- 7: Karol Mazurek, "Snake Apple VI: AMFI," Medium, 2023.
- 8: Apple Inc., "Hardened Runtime," Apple Developer Documentation, 2023.

macOS dylib 기반 공격은 광범위한 소프트웨어 공급망 보안 문제의 일부이다. ACM TOSEM의 "Research Directions in Software Supply Chain Security"(2024)는 공급망 위험도 분류 프레임워크를 제시한다[9]. arXiv:2407.00246의 "SBOM.EXE: Countering Dynamic Code Injection based on Software Bill of Materials in Java"는 Software Bill of Materials를 통해 동적 코드 주입을 감지하는 방법론을 제안한다[10]. 실제 SolarWinds Orion 공격(2020)은 빌드 인프라 침해를 통해 dependency 소스 코드에 악성 함수를 삽입했고, 18,000개 이상의 기업 고객을 감염시켰다[11]. 이는 Dylib Hijacking의 공급망 확대 변종(Framework 공급망 공격)이 이론적이 아닌 현실적 위협임을 보여준다.
MITRE ATT&CK Framework의 T1574 "Hijack Execution Flow"는 다양한 플랫폼의 동적 로딩 공격을 공식화한다[12][13]. 이 분류는 macOS Dylib Hijacking이 Windows/Linux와 동등한 위협 수준으로 인식되고 있음을 보여주며, Pegasus 스파이웨어 사례에서도 T1574 기법이 실제 활용됨을 문서화한다.
- 9: ACM, "Research Directions in Software Supply Chain Security," ACM Transactions on Software Engineering and Methodology (TOSEM), 2024.
- 10: R. Kumar et al., "SBOM.EXE: Countering Dynamic Code Injection based on Software Bill of Materials in Java," arXiv Preprint arXiv:2407.00246, July 2024.
- 11: C. Lladó et al., "SolarWinds Supply Chain Attack Analysis," arXiv Preprint arXiv:2308.10294, Aug. 2023.
- 12: MITRE ATT&CK, "Hijack Execution Flow (T1574)," ATT&CK Knowledge Base, 2024.
- 13: MITRE ATT&CK, "Hijack Execution Flow: Dylib Hijacking (T1574.004)," ATT&CK Knowledge Base, 2024.

1-3. 연구 목표 및 논문 구성
기존 도구인 DylibHijackScanner는 단순한 RPATH 탐지만 제공하며, 다음과 같은 근본적 한계를 가진다. 첫째, 다중 RPATH 체인에 대한 이해 부족이다. dyld 로더는 첫 번째 RPATH에서 라이브러리를 찾지 못하면 다음 경로들을 순차적으로 탐색한다. 기존 도구는 주로 첫 번째 RPATH만 검토하며, 뒤따르는 경로들의 공격 가능성을 간과한다. 따라서 실제 취약성의 규모를 과소 평가한다. 둘째, 공격 분류의 부재이다. RPATH 기반 단순 주입, Framework 교체 공격, 내부 의존성 체인 침해는 서로 다른 위협 수준과 미티게이션 전략을 요구하지만, 기존 연구는 이들을 동일하게 취급한다. 셋째, 실제 악용 가능성 평가의 미흡이다. 탐지된 RPATH 경로가 이론적으로는 취약하더라도, 현대 macOS의 코드 서명 검증 메커니즘(Team ID, codesign enforcement)이 실제 공격을 얼마나 저지하는지 정량화되지 않았다.
본 연구는 이러한 간극을 해결하기 위해 macOS 로컬 환경의 다중 RPATH 기반 Dylib Hijacking 취약 조건을 체계적으로 모델링하고, 자동 탐지 및 검증 플랫폼을 설계·구현하여, 실제 macOS 애플리케이션 5개를 대상으로 각 취약점 모델의 실행 가능성을 검증하였다.

본 연구가 기대하는 기여는 다음과 같다. (1) RPATH 단순 주입, Framework 교체 공격, 내부 의존성 체인 침해를 구분하는 명확한 위협 모델을 제시한다. (2) 기존 도구의 로직을 개선하여 다중 RPATH 분석의 탐지를 정량화한다. (3) 마커 파일 기반의 자동 검증 메커니즘을 제안하여 취약성과 악용 가능성을 구별한다. (4) 개발자 및 배포자를 위한 실천적 보안 가이드라인과 기업 보안팀을 위한 공급망 위험도 평가 프레임워크를 제공한다. (5) 현재 구현의 한계를 명확히 하고 향후 연구 방향을 제시한다.

본 연구는 macOS Tahoe 26.5.2 환경에서 로컬 권한을 가지고 수행되었으며, 논문의 나머지 부분은 다음과 같이 구성된다. 제2절에서는 macOS의 동적 링크 메커니즘, System Integrity Protection, 코드 서명 검증 및 기존 보안 연구를 설명한다. 제3절에서는 3가지 공격 타입의 위협 모델과 공격 시나리오를 상세히 정의한다. 제4절에서는 제안 플랫폼의 아키텍처와 각 컴포넌트를 설명하고, 제5절에서는 5개 실제 애플리케이션에 대한 검증 결과를 분석한다. 제6절에서는 개발자 및 기업 사용자를 위한 보안 가이드라인을 제시하며, 제7절에서는 Phase 2 연구 방향 및 Gatekeeper 우회 연구의 필요성을 논의한다.