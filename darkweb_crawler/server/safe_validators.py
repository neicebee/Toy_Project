"""
안전한 접근성 검증 모듈
curl과 SOCKS5를 사용해 안전하게 .onion 도메인 검증
"""

import subprocess
import logging
import re
import time
from typing import Dict

logger = logging.getLogger(__name__)

class SafeAccessibilityValidator:
    """안전한 도메인 접근성 검증 (curl SOCKS5)"""
    
    def __init__(self, tor_host: str = '127.0.0.1', tor_port: int = 9050, timeout: int = 15):
        self.tor_host = tor_host
        self.tor_port = tor_port
        self.timeout = timeout
        logger.info(f"SafeAccessibilityValidator 초기화: {tor_host}:{tor_port}")
    
    def check_accessibility(self, domain: str) -> Dict:
        """
        curl을 사용한 안전한 도메인 접근성 확인
        
        --socks5-hostname: DNS도 Tor을 통해 처리 (매우 안전!)
        -w: 상태 코드만 추출
        """
        try:
            url = f'http://{domain}'
            
            logger.info(f"접근성 검증 시작: {domain}")
            
            # curl 명령어 구성 (-w로 상태 코드 직접 추출)
            cmd = [
                'curl',
                '-m', str(self.timeout),  # timeout
                '--socks5-hostname', f'{self.tor_host}:{self.tor_port}',
                '-A', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
                '-L',  # 리다이렉트 따라가기
                '-s',  # silent
                '-w', '%{http_code}',  # 상태 코드만 출력
                '-o', '/dev/null',  # 응답 본문 무시
                url
            ]
            
            # curl 실행
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout + 5
            )
            
            # 상태 코드 추출
            status_text = result.stdout.strip()
            status_code = None
            
            if status_text and status_text.isdigit():
                status_code = int(status_text)
            
            # 접근성 판단
            accessible = status_code is not None and 200 <= status_code < 500
            
            if status_code:
                logger.info(f"접근성 결과: {domain} - {status_code}")
            else:
                logger.warning(f"접근성 확인 실패: {domain} (응답 없음)")
                if result.stderr:
                    logger.debug(f"curl 에러: {result.stderr}")
            
            return {
                'domain': domain,
                'is_accessible': accessible,
                'status_code': status_code,
                'response_time': None,
                'method': 'HEAD',
                'downloaded_bytes': 0,
                'error': None
            }
        
        except subprocess.TimeoutExpired:
            logger.warning(f"타임아웃: {domain}")
            return {
                'domain': domain,
                'is_accessible': False,
                'status_code': None,
                'response_time': self.timeout,
                'method': 'HEAD',
                'downloaded_bytes': 0,
                'error': 'Timeout'
            }
        
        except Exception as e:
            logger.error(f"검증 오류 ({domain}): {str(e)}")
            return {
                'domain': domain,
                'is_accessible': False,
                'status_code': None,
                'response_time': None,
                'method': 'HEAD',
                'downloaded_bytes': 0,
                'error': str(e)
            }
    
    def check_accessibility_specific(self, domain: str, reason: str = "") -> Dict:
        """
        특정 도메인 재확인 (도메인 업데이트 시나리오용)
        
        Args:
            domain: 확인할 도메인
            reason: 재확인 이유 (예: "색인 결과 확인", "301 리다이렉트 대응")
        
        Returns:
            is_valid: True if status_code == 200, else False
        """
        logger.info(f"도메인 재확인 시작: {domain} ({reason})")
        result = self.check_accessibility(domain)
        
        # 정확히 200 코드만 "유효"로 판정 (도메인 업데이트용)
        result['is_valid'] = result['status_code'] == 200
        result['recheck_reason'] = reason
        
        if result['is_valid']:
            logger.info(f"✅ 도메인 유효 확인됨: {domain}")
        else:
            logger.warning(f"❌ 도메인 유효하지 않음: {domain} (상태: {result['status_code']})")
        
        return result
    
    def close(self):
        """정리 작업"""
        logger.info("SafeAccessibilityValidator 종료")
