# 🖥️ Ubuntu 원격 서버 설정 가이드

Ubuntu 192.168.64.7 서버를 다크웹 검증 서버로 설정하는 상세 방법

---

## 📋 서버 사양

- **OS**: Ubuntu 22.04 LTS
- **IP**: 192.168.64.7
- **사용자**: maninblack
- **Tor**: 포트 9050 (SOCKS5)

---

## 1️⃣ 자동 설정 (권장, 3분)

```bash
# server/ 디렉터리를 서버로 업로드
scp -r server maninblack@192.168.64.7:~/

# SSH 접속 후 자동 설정 스크립트 실행
ssh maninblack@192.168.64.7
bash ~/server/setup.sh

# 완료 후 서버 시작
cd ~/server && python3 app.py
```

---

## 2️⃣ 수동 설정 (상세)

### 2-1. 필수 패키지 설정

```bash
# Tor 설치
sudo apt-get update
sudo apt-get install tor curl -y

# Tor 시작
sudo systemctl start tor
sudo systemctl enable tor

# Tor 상태 확인
sudo systemctl status tor
```

### 2-2. Python 환경 설정

```bash
# Python 3.8+
python3 --version  # 3.8 이상 확인

# pip 업그레이드
pip3 install --upgrade pip

# venv 생성 (선택)
python3 -m venv server_env
source server_env/bin/activate

# Flask 및 의존성 설치
cd ~/server
pip3 install -r requirements.txt

# 설치 확인
python3 -c "import flask; print(f'Flask {flask.__version__}')"
```

### 2-3. 서버 구성 확인

```bash
# server/config.py 검토
cat ~/server/config.py

# Tor SOCKS5 포트 확인
ss -tlnp | grep 9050  # 0.0.0.0:9050 LISTEN 확인
```

### 2-4. 서버 시작

```bash
# 포그라운드 실행 (테스트용)
cd ~/server
python3 app.py

# 백그라운드 실행 (운영용)
nohup python3 app.py > /var/log/validator/validator_server.log 2>&1 &

# 프로세스 확인
ps aux | grep app.py
```

---

## 3️⃣ 헬스 체크

### 로컬에서 테스트 (macOS)

```bash
# 헬스 체크
curl http://192.168.64.7:5000/health
# {"status": "healthy", "timestamp": "2026-04-03T..."}

# 검증 API 테스트
curl -X POST http://192.168.64.7:5000/api/validate \
  -H "Content-Type: application/json" \
  -d '{
    "domains": ["protonmailrmez3lotccipshtkleegetolb73fuirgj7r4o4vfu7ozyd.onion"],
    "checks": ["accessibility"]
  }'
```

### 서버에서 로컬 테스트

```bash
# 서버 내에서 테스트
curl http://localhost:5000/health

# Tor 연결 테스트
curl -x socks5h://localhost:9050 https://check.torproject.org/
```

---

## 4️⃣ 모니터링 및 로깅

### 로그 파일 설정

```bash
# 로그 디렉터리 생성
mkdir -p /var/log/validator

# 소유권 설정
sudo chown maninblack:maninblack /var/log/validator
sudo chmod 755 /var/log/validator
```

### 실시간 로그 모니터링

```bash
# 로컬에서
ssh maninblack@192.168.64.7 "tail -f /var/log/validator/validator_server.log"

# 또는 서버에서
tail -f /var/log/validator/validator_server.log
```

---

## 5️⃣ 보안 설정

### 방화벽 설정

```bash
# UFW 활성화 (이미 설정된 경우 스킵)
sudo ufw enable

# Flask 포트 허용 (로컬 네트워크만)
sudo ufw allow from 192.168.64.0/24 to any port 5000

# SSH 포트 허용
sudo ufw allow from any to any port 22

# 상태 확인
sudo ufw status
```

### Fail2ban 설정 (선택)

```bash
# 설치
sudo apt-get install fail2ban -y

# SSH 설정
sudo systemctl start fail2ban
sudo systemctl enable fail2ban

# 상태 확인
sudo fail2ban-client status
```

---

## 6️⃣ systemd 서비스 등록 (운영용)

### 서비스 파일 생성

```bash
sudo nano /etc/systemd/system/darkweb-validator.service
```

### 파일 내용

```ini
[Unit]
Description=Darkweb Validator Server
After=network.target tor.service

[Service]
Type=simple
User=maninblack
WorkingDirectory=/home/maninblack/server
ExecStart=/usr/bin/python3 app.py
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
```

### 활성화

```bash
# 리로드
sudo systemctl daemon-reload

# 활성화
sudo systemctl enable darkweb-validator.service

# 시작
sudo systemctl start darkweb-validator.service

# 상태 확인
sudo systemctl status darkweb-validator.service

# 로그 확인
sudo journalctl -u darkweb-validator.service -f
```

---

## 7️⃣ 성능 최적화

### Tor 설정 (torrc)

```bash
sudo nano /etc/tor/torrc
```

**권장 설정:**

```conf
# SOCKS5 포트
SocksPort 0.0.0.0:9050

# 동시 연결 허용
ConnLimit 100

# 캐시 설정
CacheExitPolicy 1
MaxCircuitDirtiness 600
```

### 재시작

```bash
sudo systemctl restart tor
```

---

## 🔧 트러블슈팅

### Tor 연결 실패

```bash
# Tor 상태 확인
sudo systemctl status tor

# Tor 로그 확인
tail -f /var/log/tor/log

# Tor 재시작
sudo systemctl restart tor
```

### socks5h 포트 확인

```bash
# 포트 열려있는지 확인
netstat -tlnp | grep 9050

# 또는
ss -tlnp | grep 9050
```

### Flask 에러

```bash
# 직접 실행해서 에러 확인
cd ~/server
python3 app.py

# 의존성 확인
python3 -c "import requests; import flask; print('OK')"
```

---

## ✅ 체크리스트

- [ ] Ubuntu 22.04 LTS
- [ ] Tor 설치 및 실행 중
- [ ] Python 3.8+ 설치
- [ ] requirements.txt 의존성 설치
- [ ] 헬스 체크 성공 (200 OK)
- [ ] socks5h://localhost:9050 응답 확인
- [ ] 로그 디렉터리 생성
- [ ] (선택) systemd 서비스 등록

---

## 📞 지원

- 로그: `/var/log/validator/validator_server.log`
- 상태: `sudo systemctl status darkweb-validator.service`
- API 문서: [API_SCHEMA.md](API_SCHEMA.md)

---

**마지막 업데이트**: 2026-04-03
