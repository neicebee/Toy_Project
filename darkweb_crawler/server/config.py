"""
원격 검증 서버 설정 - 단일 도메인 분석용
"""

import os
from datetime import datetime

# 서버 설정
SERVER_HOST = '0.0.0.0'
SERVER_PORT = 5000
SERVER_DEBUG = False

# Tor 설정
TOR_SOCKS5_HOST = '127.0.0.1'
TOR_SOCKS5_PORT = 9050

# HTTP 요청 설정
REQUEST_TIMEOUT = 40  # 초 (HTML 크롤링용)
USER_AGENT = 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'

# DDOS 필터 설정
DDOS_FILTER_RETRY_DELAY = 30
DDOS_FILTER_MAX_RETRIES = 5
REQUEST_DELAY_BETWEEN_RETRIES = 5
TOR_IDENTITY_CHANGE_ENABLED = True
TOR_CONTROL_PORT = 9051
USE_ROTATING_USER_AGENTS = True

# 보안 설정
SECRET_KEY = 'darkweb-scanner-secret-2024'
ALLOWED_CLIENTS = ['192.168.64.0/24']

# 로깅 설정
LOG_DIR = os.path.join(os.path.dirname(__file__), '..', 'logs')
LOG_LEVEL = 'INFO'
LOG_FORMAT = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
LOG_FILE = os.path.join(LOG_DIR, f'server_{datetime.now().strftime("%Y%m%d_%H%M%S")}.log')

# 감사 추적
AUDIT_LOG_FILE = os.path.join(LOG_DIR, 'audit_trail.log')
AUDIT_TRAIL_ENABLED = True

# Ahmia API
AHMIA_API_URL = "https://ahmia.fi/search/"

# 디렉토리 생성
os.makedirs(LOG_DIR, exist_ok=True)
