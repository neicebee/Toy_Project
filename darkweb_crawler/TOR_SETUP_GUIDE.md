# Tor Control Port 설정 가이드

## 문제
```
netstat -tlnp | grep 9051
(Not all processes could be identified, non-owned process info will not be shown)
```
→ Tor Control Port 9051이 활성화되지 않음

## 해결책

### 1️⃣ Ubuntu 서버 - Tor torrc 설정 수정

```bash
# Tor 설정 파일 백업
sudo cp /etc/tor/torrc /etc/tor/torrc.backup

# 설정 파일 편집 (root 필요)
sudo nano /etc/tor/torrc
```

### 2️⃣ 다음 줄 찾아 주석 해제 또는 추가

```conf
# 기존 설정 확인
SocksPort 9050
DNSPort 5353

# ❌ 주석된 상태 (찾아서 수정)
# ControlPort 9051

# ✅ 다음과 같이 변경
ControlPort 9051
CookieAuthentication 1
```

### 3️⃣ Tor 재시작

```bash
# Tor 서비스 재시작
sudo systemctl restart tor

# 포트 확인
sudo netstat -tlnp | grep tor
# 또는
sudo lsof -i :9051
```

### 4️⃣ 권한 설정 (선택사항)

```bash
# maninblack 사용자가 Tor 컨트롤할 수 있도록 설정
sudo usermod -a -G debian-tor maninblack

# 그룹 권한 적용 (재로그인 필요)
newgrp debian-tor
```

### 5️⃣ 검증

```bash
# Python에서 직접 테스트
python3 << 'EOF'
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    sock.connect(('127.0.0.1', 9051))
    sock.send(b"AUTHENTICATE \"\"\r\n")
    resp = sock.recv(1024)
    print(f"✅ Tor Control Port 연결 성공: {resp}")
except Exception as e:
    print(f"❌ 연결 실패: {e}")
finally:
    sock.close()
EOF
```

---

## Control Port 없이 작동하기 (대안)

**Control Port가 설정되지 않아도 이제 자동으로 작동합니다:**

1. **Tor Identity 변경 시도** → 실패
2. **자동 폴백** → 더 긴 대기 시간만으로 IP 변경 유도
3. **계속 진행** → 403 재시도 로직 실행

```
시도 1: 30초 대기 (IP 자동 변경 기대)
시도 2: 60초 대기
시도 3: 120초 대기 (2분)
...
```

---

## 설정 값 참고

| 포트 | 용도 | 상태 |
|------|------|------|
| 9050 | SOCKS 프록시 | ✅ 활성화 |
| 5353 | DNS | ✅ 활성화 |
| 9051 | Control Port | ❌ 필요 (선택) |

**9051 활성화 시 장점:**
- 빠른 IP 변경 (몇 초)
- DDOS 필터 우회 효율 증가

**9051 없이도:**
- 자동 IP 변경 (수십 초)
- 더 긴 대기 시간 필요
- 성능 저하 가능

---

## 트러블슈팅

### Tor 컨트롤 포트 여전히 안 됨

```bash
# Tor 상태 확인
sudo systemctl status tor

# Tor 로그 확인
sudo tail -f /var/log/tor/log

# Tor 수동 시작 (디버그 모드)
sudo tor -f /etc/tor/torrc -q
```

### 권한 오류

```bash
# Tor가 ControlSocket을 생성할 때 권한 확인
ls -la /var/run/tor/
sudo chown debian-tor:debian-tor /var/run/tor/
```

---

## 설정 완료 후

**server/app.py 자동 감지:**

```
Tor Control Port 연결 감지...
✅ Tor 식별성 변경 완료 (새로운 IP 할당)
   ⏳ 2초 대기 중...
```

**성공하면 403 필터 우회가 훨씬 빨라집니다!**
