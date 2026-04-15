# 🚀 원격 분석 5분 시작 가이드

**목표**: Ubuntu 서버의 Tor 프록시를 이용해 안전하게 .onion 도메인을 종합 분석하기

---

## 📋 사전 준비물

- ✅ macOS 로컬 머신 (Python 3.8+)
- ✅ Ubuntu 서버 (192.168.64.7)
- ✅ SSH 접근권한 (maninblack@192.168.64.7)
- ✅ Tor 설치됨 (SOCKS5 포트 9050)
- ✅ Tor Control Port 9051 활성화

---

## ⚡ 빠른 시작 (총 5분)

### 1단계: 로컬 환경 설정 (1분)

```bash
# Python 환경 생성
conda create -n toyproject python=3.11 -y
conda activate toyproject

# 프로젝트 이동
cd ~/Desktop/Toy_Project/darkweb_crawler

# 의존성 설치
pip install -r requirements.txt
```

### 2단계: 원격 서버 실행 (2분)

**Option A: SSH 원격 실행**
```bash
# Ubuntu 서버 로그인
ssh maninblack@192.168.64.7

# 서버 시작 (백그라운드)
cd ~/darkweb_crawler
nohup python3 server/app.py > /tmp/server.log 2>&1 &

# 헬스 체크
curl http://localhost:5000/health
```

**Option B: 로컬에서 SSH 포트포워딩**
```bash
ssh -L 5000:localhost:5000 maninblack@192.168.64.7
# 다른 터미널에서
python3 server/app.py
```

### 3단계: 단일 도메인 분석 (2분)

**CLI 모드**
```bash
python3 agent.py

# 입력:
# 도메인: dreadytofvwu4oa6.onion

# 출력:
✅ 분석 완료!
📊 신뢰도: 78/100 (moderate_trust)
📁 보고서: analysis_reports/dreadytofvwu4oa6.onion_2026-04-15.html
```

**웹 모드**
```bash
python3 launcher.py
# → 옵션 2 선택 (웹 모드)
# → http://localhost:5001 로 접속
# → 도메인 입력 후 분석 시작
```

---

## 📱 API 직접 호출

### Python 클라이언트
```python
import requests

# 원격 서버 분석
response = requests.post(
    'http://192.168.64.7:5000/api/analyze_domain',
    json={'domain': 'dreadytofvwu4oa6.onion'}
)

result = response.json()
print(f"신뢰도: {result['analysis_result']['trust_score']}")
print(f"카테고리: {result['analysis_result']['category_result']['primary_category']}")
```

### cURL 테스트
```bash
curl -X POST http://192.168.64.7:5000/api/analyze_domain \
  -H "Content-Type: application/json" \
  -d '{"domain": "dreadytofvwu4oa6.onion"}' | jq .
```

---

## 🔍 결과 확인

### 분석 보고서 위치
```bash
ls -la analysis_reports/
# dreadytofvwu4oa6.onion_2026-04-15_14-23-45.html (15KB)
```

### 보고서 열기
```bash
open analysis_reports/dreadytofvwu4oa6.onion_*.html
```

---

## 🚨 문제 해결

### 서버 연결 오류
```bash
# 원격 서버 상태 확인
ssh maninblack@192.168.64.7 "curl http://localhost:5000/health"

# 포트 열려있는지 확인
ssh maninblack@192.168.64.7 "netstat -tlnp | grep 5000"

# 서버 로그 확인
ssh maninblack@192.168.64.7 "tail -50 /tmp/server.log"
```

### Tor 연결 오류
```bash
# 원격 Tor 상태 확인
ssh maninblack@192.168.64.7 "curl --socks5 127.0.0.1:9050 https://check.torproject.org"

# Tor 재시작
ssh maninblack@192.168.64.7 "sudo systemctl restart tor"
```

### 도메인 분석 실패
```bash
# 로컬 로그 확인
tail -50 logs/__main__.log

# 자세한 로그 모드
python3 agent.py --debug
```

---

## 📋 스크립트로 배치 분석

### 여러 도메인 순차 분석
```bash
cat > batch_analyze.py << 'SCRIPT'
#!/usr/bin/env python3
import requests
import time

domains = [
    'dreadytofvwu4oa6.onion',
    'cryptbbs7cvk2fpjz.onion',
    'example.onion'
]

api_url = 'http://192.168.64.7:5000/api/analyze_domain'

for domain in domains:
    print(f"\n🔍 분석 중: {domain}")
    response = requests.post(api_url, json={'domain': domain})
    
    if response.status_code == 200:
        result = response.json()
        score = result['analysis_result']['trust_score']
        print(f"✅ 신뢰도: {score}/100")
    else:
        print(f"❌ 오류: {response.status_code}")
    
    time.sleep(2)  # 요청 간격
SCRIPT

python3 batch_analyze.py
```

---

## 📊 성능 지표

| 작업 | 소요시간 | 설명 |
|------|---------|------|
| 단일 도메인 분석 | 8-15초 | Tor 프록시 + HTML 크롤링 포함 |
| 색인 확인 | 2-3초 | Ahmia + DuckDuckGo 검색 |
| 콘텐츠 분석 | 3-5초 | 불법 콘텐츠 탐지 |
| 보고서 생성 | 1-2초 | HTML 차트 + 통계 |

---

**마지막 업데이트**: 2026년 4월 15일
