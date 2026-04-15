"""
검색엔진 색인 여부 확인 (Ahmia - Curl 기반, 토큰 캐싱)
한 번만 Ahmia 홈페이지에서 hidden input 토큰을 추출하고 캐싱하여 재사용
name="XXXXXX" value="XXXXXX" 형식의 숨겨진 값을 사용
"""

import subprocess
import logging
from typing import Dict, Tuple
import re
from datetime import datetime, timedelta

logger = logging.getLogger(__name__)

class IndexingValidator:
    """Ahmia를 통한 색인 여부 확인 (토큰 캐싱)"""
    
    def __init__(self):
        self.timeout = 25  # 일부 도메인이 9초+ 걸리므로 여유 있게 설정
        self.ahmia_home = "https://ahmia.fi/"
        self.search_url = "https://ahmia.fi/search/"
        
        # 토큰 캐시 (한 번만 추출)
        self.cached_token = None  # (name=value 형식)
        self.token_expire_time = None  # 토큰 만료 시간
        self.token_cache_duration = 600  # 10분 캐시
    
    def _extract_domain_from_html(self, html: str) -> str:
        """
        Ahmia 검색 결과 HTML에서 첫 번째 .onion 도메인 추출
        
        반환:
            도메인 (예: "example.onion") 또는 None
        """
        try:
            # 패턴 1: href 속성에서 도메인 추출
            # 예: <a href="http://example.onion">...</a>
            href_pattern = r'href=["\'](?:https?://)?([a-z0-9]+\.onion)["\']'
            match = re.search(href_pattern, html, re.IGNORECASE)
            if match:
                domain = match.group(1).lower()
                logger.info(f"✅ 도메인 추출 성공 (href): {domain}")
                return domain
            
            # 패턴 2: 텍스트 콘텐츠에서 도메인 추출
            # 예: <article>...<h3>example.onion</h3>...</article>
            text_pattern = r'>([a-z0-9]+\.onion)<'
            match = re.search(text_pattern, html, re.IGNORECASE)
            if match:
                domain = match.group(1).lower()
                logger.info(f"✅ 도메인 추출 성공 (텍스트): {domain}")
                return domain
            
            # 패턴 3: 링크 텍스트에서 도메인 추출
            # 예: <a>example.onion</a>
            link_pattern = r'>([a-z0-9]+-?[a-z0-9]+\.onion)</a>'
            match = re.search(link_pattern, html, re.IGNORECASE)
            if match:
                domain = match.group(1).lower()
                logger.info(f"✅ 도메인 추출 성공 (링크): {domain}")
                return domain
            
            logger.warning("❌ 검색 결과에서 도메인을 찾을 수 없음 (HTML 형식 확인 필요)")
            return None
        
        except Exception as e:
            logger.error(f"❌ 도메인 추출 오류: {str(e)}")
            return None
    
    def _extract_urls_from_html(self, html: str, extracted_domain: str = None) -> list:
        """
        Ahmia 검색 결과에서 전체 URL 추출 (도메인 + 경로)
        
        Args:
            html: 검색 결과 HTML
            extracted_domain: 추출된 메인 도메인
        
        Returns:
            전체 URL 목록 (예: ['example.onion', 'example.onion/page/about', ...])
        """
        urls = []
        
        try:
            if not extracted_domain:
                extracted_domain = self._extract_domain_from_html(html)
            
            if not extracted_domain:
                return urls
            
            # 1단계: 절대 경로 href에서 추출 (http://example.onion/path)
            absolute_href_pattern = r'href=["\']https?://([a-z0-9]+\.onion)(/[^"\']*?)["\']'
            for match in re.finditer(absolute_href_pattern, html, re.IGNORECASE):
                domain = match.group(1).lower()
                path = match.group(2).lower().strip()
                
                # "#" 이전까지만 추출 (페이지 내 앵커 제외)
                if '#' in path:
                    path = path.split('#')[0]
                
                # 쿼리스트링 이전까지만 추출
                if '?' in path:
                    path = path.split('?')[0]
                
                if path and path != '/':
                    url = f"{domain}{path}"
                else:
                    url = domain
                
                if url not in urls:
                    urls.append(url)
                    logger.debug(f"  📌 URL 추출 (절대경로): {url}")
            
            # 2단계: 상대 경로는 제외 (실제 도메인 HTML에서만 추출)
            # Ahmia 검색 결과의 상대 경로는 Ahmia 자신의 페이지이므로 불필요
            
            # 3단계: 메인 도메인 (패턴 매칭 없이 직접)
            main_url = f"http://{extracted_domain}"
            if main_url not in urls:
                urls.insert(0, main_url)
            
            logger.info(f"✅ 추출된 URL (Ahmia 절대경로): {len(urls)}개")
            
            return urls
        
        except Exception as e:
            logger.error(f"❌ URL 추출 오류: {str(e)}")
            return []
    
    def _extract_token_from_html(self, html: str) -> str:
        """
        HTML에서 hidden input 토큰 추출
        
        예: <input type="hidden" name="d273c3" value="77a16c">
        반환: "d273c3=77a16c"
        """
        try:
            # Hidden input 태그에서 name과 value 추출
            match = re.search(
                r'<input[^>]*type=["\']?hidden["\']?[^>]*name=["\']?([a-f0-9]{6})["\']?[^>]*value=["\']?([a-f0-9]{6})["\']?',
                html,
                re.IGNORECASE
            )
            
            if match:
                name = match.group(1)
                value = match.group(2)
                token = f"{name}={value}"
                logger.info(f"Hidden input 토큰 추출: {token}")
                return token
            
            logger.warning("Hidden input 토큰을 찾을 수 없음")
            return None
        
        except Exception as e:
            logger.error(f"토큰 추출 오류: {str(e)}")
            return None
    
    def _get_token(self) -> str:
        """
        토큰 획득 (캐시되어 있으면 재사용, 만료되었으면 새로 추출)
        
        Returns:
            토큰 (예: "d273c3=77a16c") 또는 None
        """
        # 캐시된 토큰이 유효한지 확인
        if self.cached_token and self.token_expire_time:
            if datetime.now() < self.token_expire_time:
                logger.debug(f"캐시된 토큰 사용: {self.cached_token}")
                return self.cached_token
        
        # 토큰 추출 (새로운 토큰 필요)
        logger.info("Ahmia 홈페이지에서 새 토큰 추출 중...")
        
        try:
            cmd = [
                'curl',
                '-L',  # 리다이렉트 따라가기
                '-s',  # 침묵 모드
                '-m', str(self.timeout),
                '-A', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
                self.ahmia_home
            ]
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                timeout=self.timeout + 5,
                text=True
            )
            
            if result.returncode != 0:
                logger.warning(f"Ahmia 홈페이지 접속 실패: {result.stderr}")
                return None
            
            html = result.stdout
            token = self._extract_token_from_html(html)
            
            if token:
                # 토큰 캐시 저장
                self.cached_token = token
                self.token_expire_time = datetime.now() + timedelta(seconds=self.token_cache_duration)
                logger.info(f"토큰 캐시 저장 (10분 유효): {token}")
                return token
            
            return None
        
        except Exception as e:
            logger.error(f"토큰 획득 오류: {str(e)}")
            return None
    
    def check_indexing(self, domain: str) -> Dict:
        """
        도메인이 Ahmia 검색엔진에 색인되었는지 확인
        캐시된 토큰을 사용하여 효율적으로 검색
        
        주의: .onion 도메인은 보안상 주기적으로 변경되므로,
        모든 도메인을 동일하게 실제로 Ahmia API로 검색합니다.
        """
        try:
            domain_clean = domain.lower().replace('http://', '').replace('https://', '')
            
            logger.info(f"색인 여부 확인: {domain_clean}")
            
            # 캐시된 토큰 획득 (첫 번만 추출, 이후엔 캐시된 값 사용)
            token = self._get_token()
            
            if not token:
                logger.warning(f"토큰 없음, 검색 건너뜀: {domain_clean}")
                return {
                    'domain': domain_clean,
                    'is_indexed': False,
                    'result_count': 0,
                    'engine': 'ahmia',
                    'error': 'Token extraction failed',
                    'extracted_domain': None
                }
            
            # 토큰을 포함한 URL 구성
            search_url = f"{self.search_url}?q={domain_clean}&{token}"
            
            logger.debug(f"Ahmia 검색 URL: {search_url}")
            
            # Curl로 Ahmia 검색 수행
            cmd = [
                'curl',
                '-L',  # 리다이렉트 따라가기
                '-s',  # 침묵 모드
                '-m', str(self.timeout),
                '-A', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
                search_url
            ]
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                timeout=self.timeout + 5,
                text=True
            )
            
            if result.returncode != 0:
                logger.warning(f"Curl 실패: {domain_clean} - {result.stderr}")
                return {
                    'domain': domain_clean,
                    'is_indexed': False,
                    'result_count': 0,
                    'engine': 'ahmia',
                    'error': f'Curl error: {result.returncode}',
                    'extracted_domain': None
                }
            
            html_response = result.stdout
            
            if not html_response or len(html_response) < 100:
                logger.warning(f"Ahmia 응답 비어있음: {domain_clean}")
                return {
                    'domain': domain_clean,
                    'is_indexed': False,
                    'result_count': 0,
                    'engine': 'ahmia',
                    'error': 'Empty response',
                    'extracted_domain': None
                }
            
            # HTML에서 검색 결과 개수 추출
            result_count = 0
            html_lower = html_response.lower()
            
            # 패턴 1: "No results found" 확인
            if 'no result' in html_lower or 'no match' in html_lower:
                result_count = 0
            else:
                # 패턴 2: "approximately X" or "of about X"
                match = re.search(r'(?:approximately|of about)\s+([0-9,]+)', html_lower)
                if match:
                    try:
                        result_count = int(match.group(1).replace(',', ''))
                    except ValueError:
                        result_count = 0
                
                # 패턴 3: 검색 결과 카드/div 개수 세기
                if result_count == 0:
                    result_divs = html_response.count('class="result"') + \
                                  html_response.count('class="search-result"') + \
                                  html_response.count('<article')
                    if result_divs > 0:
                        result_count = result_divs
            
            is_indexed = result_count > 0
            
            logger.info(f"Ahmia 결과: {domain_clean} - 색인: {is_indexed} (결과: {result_count}건)")
            
            return {
                'domain': domain_clean,
                'is_indexed': is_indexed,
                'result_count': result_count,
                'engine': 'ahmia',
                'error': None,
                'extracted_domain': None
            }
        
        except subprocess.TimeoutExpired:
            logger.warning(f"Curl 타임아웃: {domain}")
            return {
                'domain': domain,
                'is_indexed': False,
                'result_count': 0,
                'engine': 'ahmia',
                'error': 'Timeout',
                'extracted_domain': None
            }
        
        except Exception as e:
            logger.error(f"색인 확인 오류 ({domain}): {str(e)}")
            return {
                'domain': domain,
                'is_indexed': False,
                'result_count': 0,
                'engine': 'ahmia',
                'error': str(e),
                'extracted_domain': None
            }
    
    def batch_check(self, domains: list) -> list:
        """
        여러 도메인 일괄 검색 (토큰은 한 번만 추출)
        """
        results = []
        for domain in domains:
            result = self.check_indexing(domain)
            results.append(result)
        return results