"""
Tor SOCKS5 프록시 핸들러
"""

import socket
import socks
import requests
from urllib.parse import urljoin
from config.config import TOR_SOCKS5_HOST, TOR_SOCKS5_PORT, REQUEST_TIMEOUT, USER_AGENT
from utils.logger import get_logger

logger = get_logger(__name__)

class TorHandler:
    """Tor SOCKS5 연결 관리"""
    
    def __init__(self):
        self.host = TOR_SOCKS5_HOST
        self.port = TOR_SOCKS5_PORT
        self.timeout = REQUEST_TIMEOUT
        self.session = None
    
    def create_session(self) -> requests.Session:
        """Tor을 통한 requests 세션 생성"""
        try:
            # PySocks를 사용한 SOCKS5 설정
            socks.set_default_proxy(socks.SOCKS5, self.host, self.port)
            socket.socket = socks.socksocket
            
            session = requests.Session()
            session.proxies = {
                'http': f'socks5://{self.host}:{self.port}',
                'https': f'socks5://{self.host}:{self.port}'
            }
            session.headers.update({
                'User-Agent': USER_AGENT
            })
            
            self.session = session
            logger.info(f"Tor 세션 생성 성공: {self.host}:{self.port}")
            return session
        
        except Exception as e:
            logger.error(f"Tor 세션 생성 실패: {str(e)}")
            return None
    
    def get_current_ip(self) -> str:
        """현재 Tor 출구 IP 확인"""
        try:
            if not self.session:
                self.create_session()
            
            response = self.session.get(
                'http://httpbin.org/ip',
                timeout=self.timeout
            )
            
            if response.status_code == 200:
                return response.json()['origin']
            return None
        
        except Exception as e:
            logger.error(f"IP 확인 실패: {str(e)}")
            return None
    
    def request_onion_domain(self, url: str, method: str = 'GET', timeout: int = None) -> dict:
        """
        .onion 도메인에 대한 HTTP 요청
        
        Returns:
            {
                'status_code': int,
                'is_accessible': bool,
                'error': str or None,
                'response_time': float
            }
        """
        if timeout is None:
            timeout = self.timeout
        
        try:
            if not self.session:
                self.create_session()
            
            logger.info(f"요청: {url}")
            
            response = self.session.request(
                method=method,
                url=url,
                timeout=timeout,
                allow_redirects=True
            )
            
            return {
                'status_code': response.status_code,
                'is_accessible': 200 <= response.status_code < 500,
                'error': None,
                'response_time': response.elapsed.total_seconds()
            }
        
        except requests.exceptions.Timeout:
            logger.warning(f"타임아웃: {url}")
            return {
                'status_code': None,
                'is_accessible': False,
                'error': 'Timeout',
                'response_time': timeout
            }
        
        except requests.exceptions.ConnectionError:
            logger.warning(f"연결 실패: {url}")
            return {
                'status_code': None,
                'is_accessible': False,
                'error': 'ConnectionError',
                'response_time': None
            }
        
        except Exception as e:
            logger.error(f"요청 오류 ({url}): {str(e)}")
            return {
                'status_code': None,
                'is_accessible': False,
                'error': str(e),
                'response_time': None
            }
    
    def close(self):
        """세션 종료"""
        if self.session:
            self.session.close()
            logger.info("Tor 세션 종료")
