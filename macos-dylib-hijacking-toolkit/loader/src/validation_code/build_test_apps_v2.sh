#!/bin/bash

##############################################################################
# build_test_apps_v2.sh - 완전히 재작성된 빌드 스크립트
#
# 개선사항:
# 1. 단계별 명확한 오류 처리
# 2. 각 단계에서 파일 존재 확인
# 3. 상세한 디버그 메시지
# 4. Framework 구조 정확히 생성
##############################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/tmp/example_apps_build"

echo "╔════════════════════════════════════════════════════════╗"
echo "║          테스트 앱 빌드 (v2 - 완전 정리)             ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Clean start
echo "[*] 빌드 디렉토리 초기화"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
echo "    [✓] $BUILD_DIR"
echo ""

# ═══════════════════════════════════════════════════════════
# Type B: SimplePayload.framework + TestApp
# ═══════════════════════════════════════════════════════════

echo "╔════════════════════════════════════════════════════════╗"
echo "║          [Type B] SimplePayload 빌드 시작            ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Framework 디렉토리 구조 생성
echo "[Step 1] Framework 디렉토리 구조 생성"
FW_ROOT="SimplePayload.framework"
FW_A="$FW_ROOT/Versions/A"
mkdir -p "$FW_A"
mkdir -p "$FW_A/Headers"
echo "    [✓] 디렉토리 생성"
echo ""

# Step 2: SimplePayload.c 컴파일
echo "[Step 2] SimplePayload.c 컴파일"
cp "$SCRIPT_DIR/SimplePayload.c" .
cp "$SCRIPT_DIR/SimplePayload.h" .
cp "$SCRIPT_DIR/SimplePayload.h" "$FW_A/Headers/"

echo "    [*] gcc 컴파일 중..."
gcc -shared -fPIC \
    -o "$FW_A/SimplePayload" \
    SimplePayload.c \
    -I. \
    -Wall -Wextra 2>&1

if [ ! -f "$FW_A/SimplePayload" ]; then
    echo "    [ERROR] dylib 파일이 생성되지 않음!"
    exit 1
fi
echo "    [✓] dylib 생성: $FW_A/SimplePayload"

# Step 3: dylib 심볼 확인
echo "    [*] 심볼 확인"
nm "$FW_A/SimplePayload" | grep -i simple | head -3
echo "    [✓] 심볼 확인 완료"
echo ""

# Step 4: Framework 심링크 생성
echo "[Step 3] Framework 심링크 생성"
cd "$FW_ROOT/Versions"
ln -sf A Current
cd ../..
ln -sf Versions/A/SimplePayload SimplePayload.framework/SimplePayload
ln -sf Versions/A/Headers SimplePayload.framework/Headers
echo "    [✓] 심링크 생성 완료"
echo ""

# Step 5: TestApp 컴파일
echo "[Step 4] TestApp 컴파일"
mkdir -p TestApp.app/Contents/MacOS
mkdir -p TestApp.app/Contents/Frameworks

cp "$SCRIPT_DIR/test_app_type_b.c" .

echo "    [*] gcc 컴파일 중..."
gcc -o TestApp.app/Contents/MacOS/TestApp \
    test_app_type_b.c \
    "$FW_A/SimplePayload" \
    -rpath @loader_path/../Frameworks/SimplePayload.framework/Versions/A \
    -Wall -Wextra 2>&1

if [ ! -f TestApp.app/Contents/MacOS/TestApp ]; then
    echo "    [ERROR] TestApp 바이너리가 생성되지 않음!"
    exit 1
fi
echo "    [✓] TestApp 생성"

# Step 6: install_name 수정
echo "    [*] install_name 수정 중..."
install_name_tool -change "SimplePayload.framework/Versions/A/SimplePayload" \
    "@loader_path/../Frameworks/SimplePayload.framework/Versions/A/SimplePayload" \
    TestApp.app/Contents/MacOS/TestApp 2>&1 || true
echo "    [✓] install_name 수정"

# Step 7: Framework를 앱 번들에 복사
echo "[Step 5] Framework를 앱 번들에 복사"
cp -r SimplePayload.framework TestApp.app/Contents/Frameworks/
echo "    [✓] 복사 완료"

# Step 8: 검증
echo "[Step 6] 빌드 검증"
echo "    [*] 디렉토리 구조:"
find TestApp.app -type f | sort
echo "    [*] TestApp 의존성:"
otool -L TestApp.app/Contents/MacOS/TestApp | head -5
echo "    [✓] 검증 완료"
echo ""

# ═══════════════════════════════════════════════════════════
# Type C: 3단계 Framework 체인 + ModularApp
# ═══════════════════════════════════════════════════════════

echo "╔════════════════════════════════════════════════════════╗"
echo "║      [Type C] 3단계 Framework 체인 빌드 시작         ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Level3Framework (최하위)
echo "[Step 1] Level3Framework 빌드"
L3_ROOT="Level3Framework.framework"
L3_A="$L3_ROOT/Versions/A"
mkdir -p "$L3_A/Headers"

cp "$SCRIPT_DIR/Level3Framework.c" .
cp "$SCRIPT_DIR/ModularFramework.h" .
cp "$SCRIPT_DIR/ModularFramework.h" "$L3_A/Headers/"

gcc -shared -fPIC \
    -o "$L3_A/Level3Framework" \
    Level3Framework.c \
    -I. \
    -Wall -Wextra 2>&1

[ -f "$L3_A/Level3Framework" ] || { echo "[ERROR] Level3 빌드 실패"; exit 1; }

cd "$L3_ROOT/Versions"
ln -sf A Current
cd ../..
ln -sf Versions/A/Level3Framework "$L3_ROOT/Level3Framework"
echo "    [✓] Level3Framework 생성"
echo ""

# Step 2: Level2Framework (중간, Level3 의존)
echo "[Step 2] Level2Framework 빌드"
L2_ROOT="Level2Framework.framework"
L2_A="$L2_ROOT/Versions/A"
mkdir -p "$L2_A/Headers"

cp "$SCRIPT_DIR/Level2Framework.c" .
cp "$SCRIPT_DIR/ModularFramework.h" "$L2_A/Headers/"

gcc -shared -fPIC \
    -o "$L2_A/Level2Framework" \
    Level2Framework.c \
    "$L3_A/Level3Framework" \
    -rpath @loader_path/../Level3Framework.framework/Versions/A \
    -Wall -Wextra 2>&1

[ -f "$L2_A/Level2Framework" ] || { echo "[ERROR] Level2 빌드 실패"; exit 1; }

cd "$L2_ROOT/Versions"
ln -sf A Current
cd ../..
ln -sf Versions/A/Level2Framework "$L2_ROOT/Level2Framework"
echo "    [✓] Level2Framework 생성"
echo ""

# Step 3: Level1Framework (최상위, Level2 의존, 가장 위험!)
echo "[Step 3] Level1Framework 빌드"
L1_ROOT="Level1Framework.framework"
L1_A="$L1_ROOT/Versions/A"
mkdir -p "$L1_A/Headers"

cp "$SCRIPT_DIR/Level1Framework.c" .
cp "$SCRIPT_DIR/ModularFramework.h" "$L1_A/Headers/"

gcc -shared -fPIC \
    -o "$L1_A/Level1Framework" \
    Level1Framework.c \
    "$L2_A/Level2Framework" \
    -rpath @loader_path/../Level2Framework.framework/Versions/A \
    -Wall -Wextra 2>&1

[ -f "$L1_A/Level1Framework" ] || { echo "[ERROR] Level1 빌드 실패"; exit 1; }

cd "$L1_ROOT/Versions"
ln -sf A Current
cd ../..
ln -sf Versions/A/Level1Framework "$L1_ROOT/Level1Framework"
echo "    [✓] Level1Framework 생성 (★ Type C 공격의 핵심)"
echo ""

# Step 4: ModularApp 컴파일
echo "[Step 4] ModularApp 컴파일"
mkdir -p ModularApp.app/Contents/MacOS
mkdir -p ModularApp.app/Contents/Frameworks

cp "$SCRIPT_DIR/test_app_type_c.c" .

gcc -o ModularApp.app/Contents/MacOS/ModularApp \
    test_app_type_c.c \
    "$L1_A/Level1Framework" \
    -rpath @loader_path/../Frameworks/Level1Framework.framework/Versions/A \
    -Wall -Wextra 2>&1

[ -f ModularApp.app/Contents/MacOS/ModularApp ] || { echo "[ERROR] ModularApp 빌드 실패"; exit 1; }
echo "    [✓] ModularApp 생성"

# Step 5: install_name 수정
echo "    [*] install_name 수정 중..."
install_name_tool -change "Level1Framework.framework/Versions/A/Level1Framework" \
    "@loader_path/../Frameworks/Level1Framework.framework/Versions/A/Level1Framework" \
    ModularApp.app/Contents/MacOS/ModularApp 2>&1 || true
echo "    [✓] install_name 수정"

# Step 6: Framework 앱 번들에 복사
echo "[Step 5] Framework를 앱 번들에 복사"
cp -r Level1Framework.framework ModularApp.app/Contents/Frameworks/
cp -r Level2Framework.framework ModularApp.app/Contents/Frameworks/
cp -r Level3Framework.framework ModularApp.app/Contents/Frameworks/
echo "    [✓] 복사 완료"

# Step 7: 검증
echo "[Step 6] 빌드 검증"
echo "    [*] 디렉토리 구조:"
find ModularApp.app -type f | sort
echo "    [*] ModularApp 의존성:"
otool -L ModularApp.app/Contents/MacOS/ModularApp | head -5
echo "    [✓] 검증 완료"
echo ""

# ═══════════════════════════════════════════════════════════
# 완료
# ═══════════════════════════════════════════════════════════

echo "╔════════════════════════════════════════════════════════╗"
echo "║           빌드 완료! ✓✓✓                              ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

echo "[*] 결과 위치:"
echo "    - Type B: $BUILD_DIR/TestApp.app"
echo "    - Type C: $BUILD_DIR/ModularApp.app"
echo ""

echo "[*] 테스트 명령:"
echo "    # Type B (정상 실행)"
echo "    $BUILD_DIR/TestApp.app/Contents/MacOS/TestApp"
echo ""
echo "    # Type C (정상 실행)"
echo "    $BUILD_DIR/ModularApp.app/Contents/MacOS/ModularApp"
echo ""

echo "[*] 악성화 명령:"
echo "    # Type B 악성화"
echo "    bash ./loader/src/validation_code/create_malicious_payload_type_b.sh $BUILD_DIR/TestApp.app"
echo ""
echo "    # Type C 악성화"
echo "    bash ./loader/src/validation_code/create_malicious_payload_type_c.sh $BUILD_DIR/ModularApp.app"
echo ""
