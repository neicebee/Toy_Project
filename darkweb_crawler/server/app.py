"""
Flask 기반 원격 검증 서버
Ubuntu 서버에서 실행
"""

from flask import Flask, request, jsonify
from functools import wraps
import logging
import json
from datetime import datetime
import ipaddress
import sys
import os
import time
import re
import requests
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry
import urllib3

# SSL 인증서 검증 경고 억제 (.onion 도메인은 자체 서명 인증서 사용)
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# 현재 디렉토리(server/)와 부모 디렉토리를 경로에 추가
# 이렇게 하면 config.py와 utils/forum_classifier.py를 모두 찾을 수 있음
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from config import (
    SERVER_HOST, SERVER_PORT, SERVER_DEBUG,
    LOG_FILE, LOG_LEVEL, ALLOWED_CLIENTS,
    AUDIT_TRAIL_ENABLED, AUDIT_LOG_FILE,
    TOR_SOCKS5_HOST, TOR_SOCKS5_PORT, REQUEST_TIMEOUT,
    DDOS_FILTER_RETRY_DELAY, DDOS_FILTER_MAX_RETRIES,
    REQUEST_DELAY_BETWEEN_RETRIES, TOR_IDENTITY_CHANGE_ENABLED,
    TOR_CONTROL_PORT, USE_ROTATING_USER_AGENTS
)
from safe_validators import SafeAccessibilityValidator
from indexing_validator import IndexingValidator
from concealment_validator import ConcealmentValidator
from duckduckgo_client import DuckDuckGoClient
from utils.forum_classifier import ForumClassifier

# 로깅 설정
logging.basicConfig(
    level=getattr(logging, LOG_LEVEL),
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILE, encoding='utf-8'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

app = Flask(__name__)
app.config['JSON_SORT_KEYS'] = False

# 글로벌 검증기
accessibility_validator = None
indexing_validator = None
concealment_validator = None
duckduckgo_client = None
forum_classifier = None

def init_validators():
    """검증기 초기화"""
    global accessibility_validator, indexing_validator, concealment_validator, duckduckgo_client, forum_classifier
    
    logger.info("검증기 초기화 중...")
    
    accessibility_validator = SafeAccessibilityValidator()
    indexing_validator = IndexingValidator()
    concealment_validator = ConcealmentValidator()
    duckduckgo_client = DuckDuckGoClient()
    forum_classifier = ForumClassifier()
    
    logger.info("검증기 초기화 완료")

def check_client_ip(f):
    """허용된 클라이언트 IP만 접속 허용"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        client_ip = request.remote_addr
        
        # 로컬호스트는 항상 허용
        if client_ip == '127.0.0.1' or client_ip == 'localhost':
            return f(*args, **kwargs)
        
        # 허용된 네트워크 범위 확인
        allowed = False
        for allowed_subnet in ALLOWED_CLIENTS:
            try:
                if ipaddress.ip_address(client_ip) in ipaddress.ip_network(allowed_subnet):
                    allowed = True
                    break
            except ValueError:
                logger.error(f"Invalid IP/subnet: {allowed_subnet}")
        
        if not allowed:
            logger.warning(f"허용되지 않은 클라이언트: {client_ip}")
            return jsonify({'error': 'Forbidden'}), 403
        
        return f(*args, **kwargs)
    
    return decorated_function

def change_tor_identity():
    """Tor 식별성 변경 (새로운 IP 할당)
    
    Tor 컨트롤 포트(9051)를 통해 새로운 회로를 생성합니다.
    SIGNAL NEWNYM 명령으로 Tor 프로세스가 새 IP를 할당하도록 지시합니다.
    """
    try:
        import socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        
        logger.info(f"   🔗 Tor Control Port(9051) 연결 시도...")
        sock.connect(('127.0.0.1', TOR_CONTROL_PORT))
        logger.debug(f"   ✅ TCP 연결 성공")
        
        # AUTHENTICATE 명령 (비밀번호 없음)
        logger.debug(f"   📤 AUTHENTICATE 명령 전송...")
        sock.send(b"AUTHENTICATE \"\"\r\n")
        response = sock.recv(1024)
        
        if b'250' not in response:
            logger.warning(f"   ❌ Tor 인증 실패")
            logger.debug(f"      응답: {response.decode('utf-8', errors='ignore')}")
            sock.close()
            return False
        
        logger.debug(f"   ✅ Tor 인증 성공")
        
        # SIGNAL NEWNYM 명령 (새 회로 생성)
        logger.debug(f"   📤 SIGNAL NEWNYM 명령 전송...")
        sock.send(b"SIGNAL NEWNYM\r\n")
        response = sock.recv(1024)
        sock.close()
        
        if b'250' in response:
            logger.info(f"   ✅ Tor 식별성 변경 완료 (새로운 IP 할당)")
            logger.debug(f"      응답: {response.decode('utf-8', errors='ignore').strip()}")
            time.sleep(1)  # Tor가 새 회로를 준비할 시간 제공
            return True
        else:
            logger.warning(f"   ❌ SIGNAL NEWNYM 명령 실패")
            logger.debug(f"      응답: {response.decode('utf-8', errors='ignore')}")
            return False
    
    except socket.timeout:
        logger.error(f"   ❌ Tor Control Port 연결 타임아웃 (5초)")
        return False
    
    except ConnectionRefusedError:
        logger.error(f"   ❌ Tor Control Port 연결 거부 (9051 포트 열려있지 않음)")
        logger.error(f"      해결책: Ubuntu에서 'ControlPort 9051' 설정 후 Tor 재시작")
        return False
    
    except socket.error as e:
        logger.error(f"   ❌ 소켓 오류: {str(e)}")
        return False
    
    except Exception as e:
        logger.error(f"   ❌ Tor 식별성 변경 오류: {str(e)}")
        return False

def get_html_content_via_tor(domain: str, timeout: int = 40) -> str:
    """Tor를 통해 도메인의 HTML 컨텐츠 획득
    
    Args:
        domain: .onion 도메인
        timeout: 요청 타임아웃 (초)
    
    Returns:
        HTML 컨텐츠 또는 빈 문자열
    """
    session = None
    try:
        # Tor SOCKS5 프록시 설정 (socks5h = hostname resolution through Tor)
        proxies = {
            'http': f'socks5h://{TOR_SOCKS5_HOST}:{TOR_SOCKS5_PORT}',
            'https': f'socks5h://{TOR_SOCKS5_HOST}:{TOR_SOCKS5_PORT}'
        }
        
        # 세션 생성 (강화된 재시도 로직)
        session = requests.Session()
        
        # 재시도 전략: DNS 해석 오류도 포함
        retry = Retry(
            total=5,  # 총 5회 재시도
            backoff_factor=0.5,  # 0.5, 1.0, 2.0, 4.0, 8.0초
            status_forcelist=[429, 500, 502, 503, 504],
            allowed_methods=['GET', 'HEAD']
        )
        
        adapter = HTTPAdapter(
            max_retries=retry,
            pool_connections=1,
            pool_maxsize=1
        )
        session.mount('http://', adapter)
        session.mount('https://', adapter)
        
        # Keep-alive 비활성화 (Tor 호환성 향상)
        session.headers.update({
            'Connection': 'close',
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
        })
        
        url = f'http://{domain}'
        
        logger.debug(f"📡 Tor를 통한 HTML 요청 시작: {url} (timeout={timeout}초)")
        logger.debug(f"   프록시: socks5://{TOR_SOCKS5_HOST}:{TOR_SOCKS5_PORT}")
        
        # 403 DDOS 필터 우회용 재시도 루프
        for ddos_retry in range(DDOS_FILTER_MAX_RETRIES):
            try:
                response = session.get(
                    url,
                    proxies=proxies,
                    timeout=(5, timeout),  # (connect_timeout, read_timeout)
                    allow_redirects=True,
                    verify=False  # .onion 도메인 자체 서명 인증서 허용
                )
                
                # 403 Forbidden (DDOS 필터)
                if response.status_code == 403:
                    ddos_retry_count = ddos_retry + 1
                    logger.error(f"🚫 403 DDOS 필터 감지 (시도 {ddos_retry_count}/{DDOS_FILTER_MAX_RETRIES})")
                    logger.error(f"   응답: {response.text[:150]}")
                    
                    if ddos_retry_count < DDOS_FILTER_MAX_RETRIES:
                        logger.info(f"   🔄 [1/4] 세션 리셋 중...")
                        session.close()
                        session = requests.Session()
                        
                        # User-Agent 회전
                        if USE_ROTATING_USER_AGENTS:
                            user_agents = [
                                'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
                                'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36',
                                'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36',
                                'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:91.0) Gecko/20100101 Firefox/91.0',
                                'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:91.0) Gecko/20100101 Firefox/91.0',
                            ]
                            new_agent = user_agents[ddos_retry % len(user_agents)]
                            session.headers.update({'User-Agent': new_agent})
                            logger.info(f"   🔄 [2/4] User-Agent 변경: {new_agent[:50]}...")
                        
                        # 어댑터 재설정
                        retry = Retry(
                            total=3,
                            backoff_factor=1,
                            status_forcelist=[429, 500, 502, 503, 504],
                            allowed_methods=['GET']
                        )
                        adapter = HTTPAdapter(max_retries=retry)
                        session.mount('http://', adapter)
                        session.mount('https://', adapter)
                        logger.info(f"   🔄 [3/4] 어댑터 재설정 완료")
                        
                        # Tor Identity 변경
                        logger.info(f"   🔄 [4/4] Tor 식별성 변경 중...")
                        identity_changed = change_tor_identity()
                        
                        if identity_changed:
                            # Control Port 성공 - 짧은 대기
                            wait_time = 10 + (5 * ddos_retry)  # 10초, 15초, 20초...
                            logger.info(f"   ✅ Tor 식별성 변경 성공 - {wait_time}초 대기")
                        else:
                            # Control Port 실패 - 긴 대기
                            wait_time = DDOS_FILTER_RETRY_DELAY * (2 ** ddos_retry)
                            logger.warning(f"   ⚠️ Tor 식별성 변경 실패 - {wait_time}초 대기 (자동 IP 변경 기대)")
                        
                        logger.error(f"   ⏳ {wait_time}초 대기 중... (이 시간에 절대 요청하지 마세요)")
                        time.sleep(wait_time)
                        logger.info(f"   ⏱️ 대기 완료 - 재시도 시작")
                        continue
                    else:
                        logger.error(f"❌ 403 DDOS 필터 최대 재시도 횟수 초과 ({DDOS_FILTER_MAX_RETRIES}회) - 포기")
                        return ""
                
                # 429 Too Many Requests
                elif response.status_code == 429:
                    logger.warning(f"⚠️ 429 Too Many Requests - 요청 간격 조정 중...")
                    wait_time = REQUEST_DELAY_BETWEEN_RETRIES * (2 ** ddos_retry)
                    logger.info(f"   ⏳ {wait_time}초 대기 후 재시도...")
                    time.sleep(wait_time)
                    continue
                
                # 정상 응답 처리
                elif response.status_code < 400:
                    html_content = response.text
                    
                    # 최종 URL 로깅 (리다이렉션 된 경우)
                    if response.url != url:
                        logger.info(f"🔄 HTTP 리다이렉션 감지: {url} → {response.url}")
                    
                    # ⏳ Access Queue 페이지 감지 (단순화)
                    is_access_queue = (
                        'dread access queue' in html_content.lower()
                        or 'awaiting forwarding' in html_content.lower()
                        or 'placed in a queue' in html_content.lower()
                        or ('queue' in html_content.lower() and 'await' in html_content.lower())
                    )
                    
                    if is_access_queue:
                        logger.warning(f"⏳ Access Queue 페이지 감지 - 세션 유지 후 15초 대기")
                        logger.info(f"   🍪 세션 쿠키 유지됨")
                        logger.info(f"   ⏰ 15초 대기 시작...")
                        
                        time.sleep(15)
                        
                        # 무조건 3회 재요청
                        max_queue_retries = 3
                        base_url = response.url.split('/index')[0] if '/index' in response.url else response.url.rstrip('/')
                        
                        for queue_attempt in range(max_queue_retries):
                            logger.info(f"   📍 재요청 시도 {queue_attempt + 1}/{max_queue_retries}...")
                            try:
                                retry_response = session.get(
                                    base_url + '/',
                                    proxies=proxies,
                                    timeout=(5, timeout + 10),
                                    allow_redirects=True,
                                    verify=False
                                )
                                
                                if retry_response.status_code < 400:
                                    retry_html = retry_response.text
                                    html_content = retry_html
                                    logger.info(f"   ✅ 재요청 성공: {domain} ({len(html_content)} bytes)")
                                else:
                                    logger.warning(f"   ⚠️ 재요청 실패 (HTTP {retry_response.status_code})")
                            except Exception as retry_error:
                                logger.warning(f"   ⚠️ 재요청 오류: {str(retry_error)}")
                            
                            # 마지막 시도가 아니면 대기
                            if queue_attempt < max_queue_retries - 1:
                                logger.info(f"   ⏳ 5초 대기 후 다시 시도...")
                                time.sleep(5)
                    
                    logger.info(f"✅ HTML 획득 완료: {domain} ({len(html_content)} bytes)")
                    return html_content
                    
                    # 기타 에러 상태 코드
                else:
                    logger.warning(f"⚠️ HTML 획득 실패: {domain} (HTTP {response.status_code})")
                    return ""
                
            except requests.exceptions.Timeout as e:
                logger.warning(f"⏱️ 타임아웃: {domain} ({timeout}초) - {str(e)}")
                if ddos_retry < DDOS_FILTER_MAX_RETRIES - 1:
                    wait_time = REQUEST_DELAY_BETWEEN_RETRIES
                    logger.info(f"   ⏳ {wait_time}초 대기 후 재시도...")
                    time.sleep(wait_time)
                    continue
                return ""
            
            except requests.exceptions.ConnectionError as e:
                logger.warning(f"🔌 연결 오류: {domain}")
                return ""
        
        # 모든 재시도 실패
        logger.error(f"❌ 모든 재시도 실패: {domain}")
        return ""
    
    except requests.exceptions.ConnectionError as e:
        error_msg = str(e)
        if 'name resolution' in error_msg.lower():
            logger.warning(f"🔌 DNS 해석 오류: {domain} - Tor가 DNS를 해석하지 못함")
            logger.info(f"   💡 해결책: Ubuntu에서 'DNSPort 5353' 추가 필요")
        else:
            logger.warning(f"🔌 연결 오류: {domain} - {error_msg}")
        return ""
    
    except Exception as e:
        logger.warning(f"📄 HTML 수집 오류: {domain} - {type(e).__name__}: {str(e)}")
        return ""
    
    finally:
        if session:
            session.close()

def audit_log(event_type: str, details: str):
    """감사 기록"""
    if AUDIT_TRAIL_ENABLED:
        try:
            with open(AUDIT_LOG_FILE, 'a', encoding='utf-8') as f:
                log_entry = {
                    'timestamp': datetime.now().isoformat(),
                    'client_ip': request.remote_addr,
                    'event_type': event_type,
                    'details': details
                }
                f.write(json.dumps(log_entry, ensure_ascii=False) + '\n')
        except Exception as e:
            logger.error(f"감사 기록 실패: {str(e)}")

@app.route('/health', methods=['GET'])
def health_check():
    """헬스 체크"""
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.now().isoformat()
    }), 200

@app.route('/api/status', methods=['GET'])
@check_client_ip
def server_status():
    """서버 상태 조회"""
    return jsonify({
        'status': 'running',
        'timestamp': datetime.now().isoformat(),
        'validators_initialized': all([
            accessibility_validator,
            indexing_validator,
            concealment_validator,
            forum_classifier
        ])
    }), 200

@app.route('/api/analyze_domain', methods=['POST'])
@check_client_ip
def analyze_domain():
    """
    단일 도메인 분석 엔드포인트 (새로운 에이전트용)
    
    요청:
    {
        "domain": "example.onion"
    }
    
    응답:
    {
        "success": true,
        "timestamp": "...",
        "domain": "example.onion",
        "analysis": {
            "accessibility": {
                "status_code": 200,
                "is_accessible": true,
                "redirect_domain": null,
                "response_time": 2.5,
                "fallback_domain": null (부분 접근 불가 시 추출된 도메인),
                "fallback_accessible": true/false (재확인 결과)
            },
            "indexing": {
                "ahmia_found": true,
                "ahmia_results": 45,
                "ahmia_error": null,
                "ahmia_extracted_domain": "example.onion",
                "duckduckgo_found": true,
                "duckduckgo_error": null,
                "combined_found": true
            },
            "html_content": "<html>...</html>",
            "html_collected": true,
            "html_size": 12345,
            "analysis_warning": "서버 접근 불가 시 경고 메시지" (선택적)
        },
        "error": null
    }
    """
    try:
        data = request.get_json()
        domain = data.get('domain', '').strip()
        
        if not domain:
            return jsonify({'error': 'No domain provided'}), 400
        
        client_ip = request.remote_addr
        logger.info(f"🔍 단일 도메인 분석 시작: {client_ip} - {domain}")
        audit_log('DOMAIN_ANALYSIS_REQUEST', f"도메인 분석 요청: {domain}")
        
        analysis_result = {
            'domain': domain,
            'accessibility': None,
            'indexing': None,
            'html_content': None,
            'html_collected': False,
            'html_size': 0,
            'analysis_warning': None
        }
        
        # 1. 접근성 확인
        logger.info(f"   [1/3] 접근성 확인 중...")
        acc_result = accessibility_validator.check_accessibility(domain)
        analysis_result['accessibility'] = {
            'status_code': acc_result['status_code'],
            'is_accessible': acc_result['is_accessible'],
            'redirect_domain': acc_result.get('redirect_domain'),
            'response_time': acc_result['response_time'],
            'method': acc_result['method'],
            'fallback_domain': None,
            'fallback_accessible': None
        }
        
        logger.info(f"   ✅ 접근성 확인 완료: {acc_result['status_code']}")
        
        # 2. 색인 확인 (Ahmia + DuckDuckGo)
        logger.info(f"   [2/3] 색인 확인 중...")
        
        ahmia_result = indexing_validator.check_indexing(domain)
        duckduckgo_result = duckduckgo_client.search(domain)
        
        analysis_result['indexing'] = {
            'ahmia_found': ahmia_result['is_indexed'],
            'ahmia_results': ahmia_result['result_count'],
            'ahmia_error': ahmia_result.get('error'),
            'duckduckgo_found': duckduckgo_result['is_indexed'],
            'duckduckgo_error': duckduckgo_result.get('error'),
            'combined_found': ahmia_result['is_indexed'] or duckduckgo_result['is_indexed'],
            'ahmia_extracted_domain': ahmia_result.get('extracted_domain'),
            'extracted_urls': ahmia_result.get('extracted_urls', [])  # ✅ 추출된 URL 목록 추가
        }
        
        logger.info(f"   ✅ 색인 확인 완료: Ahmia={ahmia_result['is_indexed']}, DuckDuckGo={duckduckgo_result['is_indexed']}")
        
        # 접근성이 실패한 경우의 재검증 로직
        # 300번대는 리다이렉트이므로 성공으로 처리 (allow_redirects=True이므로 따라감)
        is_unreachable = not acc_result['is_accessible'] or (acc_result['status_code'] and acc_result['status_code'] >= 400)
        
        if is_unreachable:
            logger.info(f"   ⚠️ 도메인 접근 불가 (상태코드: {acc_result['status_code']})")
            
            # 시나리오 1: Ahmia만 있는 경우
            if ahmia_result['is_indexed'] and not duckduckgo_result['is_indexed']:
                logger.info(f"   [재검증] 시나리오 1 - Ahmia 결과 있음, DuckDuckGo 없음")
                extracted_domain = ahmia_result.get('extracted_domain')
                if extracted_domain:
                    logger.info(f"   [재검증] Ahmia 추출 도메인으로 재확인: {extracted_domain}")
                    retry_result = accessibility_validator.check_accessibility(extracted_domain)
                    if retry_result['is_accessible'] and retry_result['status_code'] and retry_result['status_code'] < 400:
                        analysis_result['accessibility']['fallback_domain'] = extracted_domain
                        analysis_result['accessibility']['fallback_accessible'] = True
                        logger.info(f"   ✅ [재검증] 성공: {extracted_domain}")
                    else:
                        analysis_result['accessibility']['fallback_domain'] = extracted_domain
                        analysis_result['accessibility']['fallback_accessible'] = False
                        logger.warning(f"   ❌ [재검증] 실패: {extracted_domain} (상태: {retry_result['status_code']})")
            
            # 시나리오 2: Ahmia + DuckDuckGo 둘 다 있는 경우 (높은 신뢰도)
            elif ahmia_result['is_indexed'] and duckduckgo_result['is_indexed']:
                logger.info(f"   [재검증] 시나리오 2 - Ahmia + DuckDuckGo 둘 다 있음 (높은 신뢰도)")
                extracted_domain = ahmia_result.get('extracted_domain')
                if extracted_domain:
                    logger.info(f"   [재검증] Ahmia 추출 도메인으로 재확인: {extracted_domain}")
                    retry_result = accessibility_validator.check_accessibility(extracted_domain)
                    if retry_result['is_accessible'] and retry_result['status_code'] and retry_result['status_code'] < 400:
                        analysis_result['accessibility']['fallback_domain'] = extracted_domain
                        analysis_result['accessibility']['fallback_accessible'] = True
                        logger.info(f"   ✅ [재검증] 성공: {extracted_domain}")
                    else:
                        analysis_result['accessibility']['fallback_domain'] = extracted_domain
                        analysis_result['accessibility']['fallback_accessible'] = False
                        logger.warning(f"   ❌ [재검증] 실패: {extracted_domain} (상태: {retry_result['status_code']})") # 있는 경우 (분석 불가 통보)
            elif not ahmia_result['is_indexed'] and duckduckgo_result['is_indexed']:
                logger.warning(f"   ⚠️ [분석 불가] 시나리오 3 - DuckDuckGo 검색 결과만 있고 직접 접근 불가")
                analysis_result['analysis_warning'] = "DuckDuckGo 검색 결과는 있으나 서버에서 직접 접근할 수 없어 전체 분석이 제한될 수 있습니다."

        
        # 3. HTML 수집 (상태코드 0이면 건너뜀)
        logger.info(f"   [3/3] HTML 수집 중...")
        
        status_code = analysis_result['accessibility']['status_code']
        extracted_urls_from_target = []
        
        # ⛔ 상태코드 0이면 HTML 수집 스킵
        if status_code == 0:
            logger.warning(f"   ⚠️ HTML 수집 건너뜀 (도메인 접근 불가: 상태코드 0)")
            analysis_result['html_collected'] = False
            analysis_result['analysis_warning'] = "도메인에 접근할 수 없어 HTML 분석을 수행하지 않았습니다."
            extracted_urls_from_target = []
        else:
            html_content = get_html_content_via_tor(domain)
            
            # 수집된 HTML 처리
            if html_content:
                analysis_result['html_content'] = html_content
                analysis_result['html_collected'] = True
                analysis_result['html_size'] = len(html_content)
                logger.info(f"   ✅ HTML 수집 완료: {len(html_content)} bytes")
                extracted_urls_from_target = []  # 배치 처리 제거로 URL 추출 안 함
            else:
                logger.warning(f"   ⚠️ HTML 수집 실패")
                extracted_urls_from_target = []
        
        # 최종 URL 목록
        analysis_result['indexing']['extracted_urls'] = extracted_urls_from_target
        
        # 응답 구성
        response_data = {
            'success': True,
            'timestamp': datetime.now().isoformat(),
            'domain': domain,
            'analysis': analysis_result,
            'error': None
        }
        
        logger.info(f"✅ 도메인 분석 완료: {domain}")
        audit_log('DOMAIN_ANALYSIS_COMPLETE', f"분석 완료: {domain}")
        
        return jsonify(response_data), 200
    
    except Exception as e:
        logger.error(f"도메인 분석 중 오류: {str(e)}", exc_info=True)
        audit_log('DOMAIN_ANALYSIS_ERROR', str(e))
        
        return jsonify({
            'success': False,
            'timestamp': datetime.now().isoformat(),
            'domain': data.get('domain', 'unknown'),
            'analysis': None,
            'error': str(e)
        }), 500

@app.errorhandler(404)
def not_found(error):
    """404 에러 처리"""
    return jsonify({'error': 'Not found'}), 404

@app.errorhandler(500)
def internal_error(error):
    """500 에러 처리"""
    logger.error(f"Internal server error: {str(error)}")
    return jsonify({'error': 'Internal server error'}), 500

if __name__ == '__main__':
    logger.info("=" * 60)
    logger.info("원격 검증 서버 시작")
    logger.info("=" * 60)
    
    # 검증기 초기화
    init_validators()
    
    # 서버 시작
    logger.info(f"Flask 서버 시작: {SERVER_HOST}:{SERVER_PORT}")
    
    try:
        app.run(
            host=SERVER_HOST,
            port=SERVER_PORT,
            debug=SERVER_DEBUG
        )
    except KeyboardInterrupt:
        logger.info("서버 종료 중...")
    except Exception as e:
        logger.error(f"서버 오류: {str(e)}", exc_info=True)
    finally:
        logger.info("서버 종료 완료")
