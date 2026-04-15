"""
DuckDuckGo 검색 엔진을 통한 .onion 도메인 색인 확인 (Curl 기반)
"""

import subprocess
import logging
import re
from typing import Dict, Tuple
from datetime import datetime, timedelta

logger = logging.getLogger(__name__)


class DuckDuckGoClient:
    """DuckDuckGo를 통한 색인 여부 확인 (검색 결과 존재 판정만 수행)"""
    
    def __init__(self):
        self.timeout = 15
        self.base_url = "https://duckduckgo.com"
        self.verify_ssl = True
    
    def search(self, onion_domain: str) -> Dict:
        """
        DuckDuckGo에서 .onion 도메인 검색 결과 존재 여부 판정
        
        Args:
            onion_domain: 검색할 .onion 도메인
        
        Returns:
            {
                'is_indexed': bool,      # 검색 결과 존재 여부
                'error': str or None     # 에러 메시지
            }
        """
        try:
            # .onion 제거 후 검색 쿼리 구성
            domain_without_onion = onion_domain.replace('.onion', '') if onion_domain.endswith('.onion') else onion_domain
            query = domain_without_onion
            
            logger.info(f"🔍 DuckDuckGo 검색 시작: {query}")
            
            # Curl 명령어 구성
            cmd = [
                'curl',
                '-s',
                '--max-time', str(self.timeout),
                '--user-agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
                '-b', '/tmp/ddgo_cookies.txt',
                '-c', '/tmp/ddgo_cookies.txt'
            ]
            
            # SSL 검증 비활성화 (필요시)
            if not self.verify_ssl:
                cmd.append('-k')
            
            # 검색 URL (DuckDuckGo 스탠다드 형태)
            search_url = f"{self.base_url}/?q={query}&t=h_&ia=web"
            cmd.append(search_url)
            
            logger.debug(f"   Curl 명령어: {' '.join(cmd[:5])}... {search_url}")
            
            # 요청 실행
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout + 5
            )
            
            if result.returncode != 0:
                logger.warning(f"⚠️ DuckDuckGo 요청 실패: {result.stderr}")
                return {
                    'is_indexed': False,
                    'error': f"Curl 에러: {result.stderr}"
                }
            
            html_content = result.stdout
            
            # 검색 결과 판정: 도메인이 검색 결과에 포함되어 있는지 확인
            # DuckDuckGo는 검색 정보만 제공하므로, 간단히 도메인이 응답에 포함됐는지 확인
            is_indexed = domain_without_onion in html_content or onion_domain in html_content
            
            logger.info(f"✅ DuckDuckGo 검색 완료: {onion_domain} - 색인={is_indexed}")
            
            return {
                'is_indexed': is_indexed,
                'error': None
            }
        
        except subprocess.TimeoutExpired:
            logger.warning(f"⏱️ DuckDuckGo 요청 타임아웃: {onion_domain}")
            return {
                'is_indexed': False,
                'error': f"타임아웃 ({self.timeout}초)"
            }
        
        except Exception as e:
            logger.error(f"❌ DuckDuckGo 검색 중 오류: {str(e)}")
            return {
                'is_indexed': False,
                'error': str(e)
            }
