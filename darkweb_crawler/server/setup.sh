#!/bin/bash
# 🚀 빠른 설정 스크립트 - Ubuntu 서버에서 한 줄로 실행

set -u  # 정의되지 않은 변수 사용 시 에러

echo "🔒 Ubuntu 22.04 LTS Tier 2 보안 설정 (자동화)"
echo "======================================"

# SSH 접속 후 이 파일 다운로드 및 실행
# curl -sSL https://[URL]/setup.sh | bash

BACKUP_FILE="/tmp/fstab.backup.$(date +%s)"

# 1. 시스템 업데이트
echo "✓ 시스템 업데이트 중..."
sudo apt-get update > /dev/null 2>&1
sudo apt-get upgrade -y > /dev/null 2>&1

# 2. 필수 패키지 설치
echo "✓ 필수 패키지 설치 중..."
sudo apt-get install -y \
  python3.11 python3-pip \
  tor curl git \
  fail2ban apparmor \
  supervisor > /dev/null 2>&1

# 3. 파이어월 설정
echo "✓ 파이어월 설정 중..."
sudo ufw default deny incoming > /dev/null 2>&1 || true
sudo ufw default allow outgoing > /dev/null 2>&1 || true
sudo ufw allow 22/tcp > /dev/null 2>&1 || true
sudo ufw allow from 192.168.64.0/24 to any port 5000 > /dev/null 2>&1 || true
sudo ufw allow from 192.168.64.0/24 to any port 9050 > /dev/null 2>&1 || true
echo "y" | sudo ufw enable > /dev/null 2>&1 || true

# 4. SSH 강화
echo "✓ SSH 보안 강화 중..."
sudo sed -i 's/#PermitRootLogin yes/PermitRootLogin no/' /etc/ssh/sshd_config 2>/dev/null || true
sudo systemctl restart sshd > /dev/null 2>&1

# 5. tmpfs noexec 마운트
echo "✓ tmpfs 마운트 설정 중..."
cp /etc/fstab "$BACKUP_FILE"
grep -v '^tmpfs.*tmp' /etc/fstab > /tmp/fstab.new || true
sudo mv /tmp/fstab.new /etc/fstab

sudo tee -a /etc/fstab > /dev/null << EOF
tmpfs    /tmp    tmpfs    defaults,noexec,nosuid,nodev,mode=1777,size=2G    0    0
tmpfs    /var/tmp    tmpfs    defaults,noexec,nosuid,nodev,mode=1777,size=1G    0    0
EOF

sudo mount -o remount,noexec,nosuid,nodev /tmp 2>/dev/null || true
sudo mount -o remount,noexec,nosuid,nodev /var/tmp 2>/dev/null || true

# 6. Fail2ban 활성화
echo "✓ Fail2ban 설정 중..."
sudo systemctl enable fail2ban > /dev/null 2>&1
sudo systemctl start fail2ban > /dev/null 2>&1

# 7. 로그 디렉토리 생성
echo "✓ 로그 디렉토리 생성 중..."
sudo mkdir -p /var/log/validator
sudo chown $USER:$USER /var/log/validator
sudo mkdir -p /etc/darkweb_crawler

# 8. Python 업그레이드
echo "✓ Python 업그레이드 중..."
python3 -m pip install --upgrade pip > /dev/null 2>&1

echo ""
echo "======================================"
echo "✅ 설정 완료!"
echo "======================================"
echo ""
echo "다음 단계:"
echo "1. macOS에서 프로젝트 파일 복사:"
echo "   scp -r ~/Desktop/Toy_Project/darkweb_crawler/server $USER@192.168.64.7:~/"
echo "   scp -r ~/Desktop/Toy_Project/darkweb_crawler/data $USER@192.168.64.7:~/"
echo ""
echo "2. 서버에서 의존성 설치:"
echo "   cd ~/server && pip install -r requirements.txt"
echo ""
echo "3. 서버 시작:"
echo "   python3 ~/server/app.py"
echo ""
echo "4. macOS에서 테스트:"
echo "   curl http://192.168.64.7:5000/health"
echo ""
echo "======================================"
