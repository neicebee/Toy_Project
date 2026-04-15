"""
설정 파일 - 단일 어니언 도메인 분석 에이전트
"""

import os
from datetime import datetime

# 기본 경로
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG_DIR = os.path.join(BASE_DIR, 'logs')

# Tor 설정
TOR_SOCKS5_HOST = '127.0.0.1'
TOR_SOCKS5_PORT = 9050
TOR_CONTROL_PORT = 9051

# HTTP 요청 설정
REQUEST_TIMEOUT = 40  # 초 (HTML 크롤링용)
USER_AGENT = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'

# 다크웹 검색엔진 API
AHMIA_API_URL = "https://ahmia.fi/search/?q={query}&format=json"

# 로깅 설정
LOG_LEVEL = 'INFO'
LOG_FORMAT = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
LOG_FILE = os.path.join(LOG_DIR, f'crawler_{datetime.now().strftime("%Y%m%d_%H%M%S")}.log')

# 감사 추적 설정
AUDIT_TRAIL_ENABLED = True
AUDIT_LOG_FILE = os.path.join(LOG_DIR, 'audit_trail.log')

# 디렉토리 자동 생성
os.makedirs(LOG_DIR, exist_ok=True)
