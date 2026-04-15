"""
사이트 카테고리 분류기 - HTML 콘텐츠 기반 사이트 목적성 분류
"""

import re
import logging
from typing import Dict, List
from collections import Counter

logger = logging.getLogger(__name__)


class CategoryClassifier:
    """HTML 콘텐츠를 분석하여 사이트 카테고리 분류"""
    
    def __init__(self, categories_config: Dict = None):
        """
        Args:
            categories_config: 카테고리별 특성 사전
        """
        self.categories_config = categories_config or self._default_categories()
    
    def _default_categories(self) -> Dict:
        """기본 카테고리 설정"""
        return {
            'marketplace': {
                'keywords': [
                    'marketplace', 'market', 'shop', 'store', 'vendor', 'seller', 'buyer',
                    'product', 'price', 'cost', 'payment', 'sale', 'buy', 'sell',
                    'item', 'inventory', 'listing', 'transaction', 'deal',
                    'cart', 'checkout', 'order', 'delivery', '마켓', '상점', '판매'
                ],
                'structural_indicators': [
                    'product.*page', 'cart', 'checkout', 'vendor profile',
                    'search.*product', 'category.*product'
                ]
            },
            'forum': {
                'keywords': [
                    'forum', 'discussion', 'board', 'thread', 'post', 'member', 'user',
                    'comment', 'reply', 'topic', 'section', 'sticky', 'pinned',
                    'moderator', 'admin', 'reputation', 'vote', 'upvote', 'downvote',
                    'quote', 'message', 'conversation', '포럼', '토론', '댓글'
                ],
                'structural_indicators': [
                    'thread', 'forum.*section', 'post.*reply', 'user.*profile',
                    'reputation.*system', 'member.*list'
                ]
            },
            'social_network': {
                'keywords': [
                    'social', 'network', 'profile', 'friend', 'follower', 'follow',
                    'feed', 'timeline', 'post', 'share', 'like', 'comment', 'user',
                    'account', 'message', 'chat', 'messenger', 'direct', 'dm',
                    'community', '소셜', '프로필', '팔로우'
                ],
                'structural_indicators': [
                    'profile.*page', 'user.*feed', 'timeline', 'friend.*request',
                    'direct.*message', 'messenger'
                ]
            },
            'communication': {
                'keywords': [
                    'communication', 'contact', 'email', 'message', 'chat', 'messenger',
                    'direct', 'dm', 'mail', 'send', 'receive', 'letter', 'envelope',
                    'talk', 'speak', 'conversation', '통신', '메시지', '채팅'
                ],
                'structural_indicators': [
                    'message.*inbox', 'send.*message', 'contact.*form',
                    'direct.*message', 'chat.*interface'
                ]
            },
            'blog': {
                'keywords': [
                    'blog', 'post', 'article', 'author', 'publish', 'date', 'time',
                    'category', 'tag', 'comment', 'archive', 'recent post',
                    'about', 'contact', 'subscribe', 'rss', 'feed',
                    '블로그', '포스팅', '글', '기사'
                ],
                'structural_indicators': [
                    'blog.*post', 'article.*content', 'post.*date', 'sidebar.*recent',
                    'archive'
                ]
            },
            'news': {
                'keywords': [
                    'news', 'article', 'breaking', 'headline', 'update', 'report',
                    'journalist', 'press', 'media', 'publication', 'story',
                    'date', 'time', 'published', 'category', 'section',
                    '뉴스', '기사', '보도'
                ],
                'structural_indicators': [
                    'news.*section', 'headline', 'article.*link', 'publish.*date',
                    'breaking.*news'
                ]
            },
            'personal_blog': {
                'keywords': [
                    'personal', 'blog', 'my', 'about me', 'bio', 'biography',
                    'page', 'portfolio', 'project', 'work', 'experience', 'skill',
                    'resume', 'cv', 'contact', 'social', 'follow',
                    '개인', '블로그', '포트폴리오'
                ],
                'structural_indicators': [
                    'about.*page', 'personal.*bio', 'portfolio.*showcase',
                    'project.*list', 'contact.*info'
                ]
            },
            'product_promotion': {
                'keywords': [
                    'product', 'service', 'software', 'tool', 'application', 'app',
                    'feature', 'benefit', 'price', 'plan', 'buy', 'download',
                    'free', 'trial', 'license', 'subscription', 'version',
                    '제품', '서비스', '구독'
                ],
                'structural_indicators': [
                    'product.*feature', 'pricing.*plan', 'download.*link',
                    'feature.*list', 'benefits'
                ]
            },
            'documentation': {
                'keywords': [
                    'documentation', 'guide', 'tutorial', 'manual', 'help', 'faq',
                    'instruction', 'api', 'reference', 'developer', 'code',
                    'example', 'version', 'changelog', 'readme', 'wiki',
                    '문서', '가이드', 'API', '튜토리얼'
                ],
                'structural_indicators': [
                    'api.*reference', 'code.*example', 'getting.*started',
                    'faq', 'help.*section', 'wiki'
                ]
            },
            'marketplace_forum_mixed': {
                # 마켓과 포럼 특성 동시 보유
                'keywords': [
                    'marketplace', 'forum', 'market', 'board', 'discussion',
                    'vendor', 'thread', 'product', 'post', 'seller', 'comment',
                    'community', 'message', 'reputation', '마켓', '포럼'
                ],
                'structural_indicators': [
                    'marketplace.*discussion', 'vendor.*forum', 'product.*discussion'
                ]
            },
            'authentication_required': {
                # 로그인 페이지 (인증 필요)
                'keywords': [
                    'login', 'sign in', 'sign up', 'register', 'password', 'username',
                    'email', 'authentication', 'auth', 'member', 'account',
                    'authorize', 'credential', 'user', 'access', 'restricted',
                    '로그인', '비밀번호', '회원가입', '사용자명', '인증'
                ],
                'structural_indicators': [
                    'login.*form', 'password.*field', 'username.*field',
                    'submit.*button', 'remember.*me', 'forgot.*password'
                ]
            }
        }
    
    def classify_content(self, html: str, verbose: bool = False) -> Dict:
        """
        HTML 콘텐츠 기반 사이트 카테고리 분류
        
        Args:
            html: 분석할 HTML
            verbose: 상세 정보 출력 여부
        
        Returns:
            {
                'primary_category': 'category_name',
                'secondary_category': 'category_name' or None,
                'confidence': float (0-1),
                'category_scores': {
                    'category_name': float,
                    ...
                },
                'reasoning': str
            }
        """
        text = self._clean_html(html)
        
        if not text:
            logger.warning("분석할 텍스트가 없음")
            return {
                'primary_category': 'unknown',
                'secondary_category': None,
                'confidence': 0.0,
                'category_scores': {},
                'reasoning': 'No text to analyze',
                'error': 'Empty content'
            }
        
        # 1단계: 로그인 페이지 감지
        login_page_confidence = self._detect_login_page(text)
        
        if login_page_confidence >= 0.7:
            # 로그인 페이지 확인됨
            logger.info(f"🔐 로그인 페이지 감지됨 (신뢰도: {login_page_confidence:.2f})")
            
            # 2단계: 로그인 페이지에서 숨겨진 정보 추출 시도
            inferred_category = self._infer_category_from_login_page(html)
            
            if inferred_category and inferred_category != 'unknown':
                logger.info(f"✅ 로그인 페이지에서 카테고리 추론: {inferred_category}")
                return {
                    'primary_category': inferred_category,
                    'secondary_category': None,
                    'confidence': login_page_confidence * 0.8,  # 추론이므로 신뢰도 낮춤
                    'category_scores': {inferred_category: login_page_confidence * 0.8},
                    'reasoning': f'로그인 페이지에서 {inferred_category} 특성 감지'
                }
            else:
                # 추론 실패 → authentication_required로 분류
                logger.info(f"❌ 로그인 페이지에서 카테고리 추론 실패")
                return {
                    'primary_category': 'authentication_required',
                    'secondary_category': None,
                    'confidence': login_page_confidence,
                    'category_scores': {'authentication_required': login_page_confidence},
                    'reasoning': '로그인 필수 페이지 (실제 콘텐츠 분석 불가)'
                }
        
        # 3단계: 일반 분류 (로그인 페이지 아닌 경우)
        category_scores = {}
        
        # 각 카테고리별 스코어 계산
        for category, config in self.categories_config.items():
            score = self._calculate_category_score(text, config)
            category_scores[category] = score
            
            if verbose and score > 0:
                logger.info(f"  [{category}] 점수: {score:.2f}")
        
        # 상위 2개 카테고리 선택
        sorted_categories = sorted(
            category_scores.items(),
            key=lambda x: x[1],
            reverse=True
        )
        
        primary_category = sorted_categories[0][0] if sorted_categories else 'unknown'
        primary_score = sorted_categories[0][1] if sorted_categories else 0.0
        
        secondary_category = None
        secondary_score = 0.0
        if len(sorted_categories) > 1 and sorted_categories[1][1] > 0:
            secondary_category = sorted_categories[1][0]
            secondary_score = sorted_categories[1][1]
        
        # 주요 카테고리의 신뢰도
        confidence = min(primary_score, 1.0)
        
        # 분류 이유 생성
        reasoning = self._generate_reasoning(
            primary_category,
            primary_score,
            secondary_category,
            secondary_score
        )
        
        result = {
            'primary_category': primary_category,
            'secondary_category': secondary_category,
            'confidence': confidence,
            'category_scores': category_scores,
            'reasoning': reasoning
        }
        
        if verbose:
            logger.info(f"분류 결과: {primary_category} (신뢰도: {confidence:.2f})")
        
        return result
    
    def _clean_html(self, html: str) -> str:
        """HTML 정제"""
        if not html:
            return ""
        
        text = html
        
        # 스크립트, 스타일 제거
        text = re.sub(r'<script[^>]*>.*?</script>', ' ', text, flags=re.IGNORECASE | re.DOTALL)
        text = re.sub(r'<style[^>]*>.*?</style>', ' ', text, flags=re.IGNORECASE | re.DOTALL)
        
        # HTML 태그 제거
        text = re.sub(r'<[^>]+>', ' ', text)
        
        # 여러 공백 정리
        text = re.sub(r'\s+', ' ', text)
        text = text.strip()
        
        return text.lower()
    
    def _detect_login_page(self, text: str) -> float:
        """
        로그인 페이지 감지
        
        Returns:
            로그인 페이지 확률 (0-1)
        """
        login_keywords = [
            'login', 'sign in', 'password', 'username', 'register', 'sign up',
            'email', 'authentication', 'auth', 'account', 'access',
            '로그인', '비밀번호', '회원가입', '사용자명', '인증'
        ]
        
        login_indicators = [
            'login.*form', 'password.*field', 'username.*field',
            'submit.*button', 'remember.*me', 'forgot.*password',
            'input.*password', 'input.*email', 'input.*username'
        ]
        
        # 키워드 점수
        keyword_score = self._calculate_keyword_score(text, login_keywords)
        
        # 구조 점수
        structural_score = self._calculate_structural_score(text, login_indicators)
        
        # 가중 합산
        login_confidence = (keyword_score * 0.6) + (structural_score * 0.4)
        
        return min(login_confidence, 1.0)
    
    def _infer_category_from_login_page(self, html: str) -> str:
        """
        로그인 페이지에서 숨겨진 정보를 통해 카테고리 추론
        
        로그인 페이지의 메타정보, 제목, 명시적 텍스트에서 카테고리 힌트를 추출합니다.
        
        Returns:
            추론된 카테고리 또는 'unknown'
        """
        # 페이지 제목에서 추론
        title_match = re.search(r'<title[^>]*>(.*?)</title>', html, re.IGNORECASE | re.DOTALL)
        if title_match:
            title = title_match.group(1).lower()
            logger.debug(f"   페이지 제목: {title}")
            
            # 마켓 관련 키워드
            if any(kw in title for kw in ['market', 'shop', 'store', 'vendor', '마켓']):
                return 'marketplace'
            # 포럼 관련 키워드
            elif any(kw in title for kw in ['forum', 'board', 'discussion', '포럼']):
                return 'forum'
            # SNS 관련 키워드
            elif any(kw in title for kw in ['social', 'network', 'community', '소셜']):
                return 'social_network'
        
        # 메타 설명에서 추론
        meta_match = re.search(r'<meta\s+name=["\']description["\']\s+content=["\'](.*?)["\']', html, re.IGNORECASE | re.DOTALL)
        if meta_match:
            meta = meta_match.group(1).lower()
            logger.debug(f"   메타 설명: {meta}")
            
            if any(kw in meta for kw in ['marketplace', 'trading', 'vendor', 'product', '마켓']):
                return 'marketplace'
            elif any(kw in meta for kw in ['forum', 'community', 'discussion', '포럼']):
                return 'forum'
            elif any(kw in meta for kw in ['social', 'social network', 'sns']):
                return 'social_network'
        
        # 페이지 본문의 명시적 설명 텍스트에서 추론
        # (로그인 폼 근처에 있을 가능성 있음)
        text = self._clean_html(html)
        
        # 페이지 초반 부분에서만 검색 (앞 2000글자)
        preview_text = text[:2000]
        
        if any(kw in preview_text for kw in ['marketplace', 'trading platform', 'vendor', 'seller', '마켓플레이스']):
            return 'marketplace'
        elif any(kw in preview_text for kw in ['forum', 'community', 'discussion board', '포럼']):
            return 'forum'
        elif any(kw in preview_text for kw in ['social network', 'social platform', 'sns', '소셜']):
            return 'social_network'
        
        return 'unknown'
    def _calculate_category_score(self, text: str, config: Dict) -> float:
        """
        카테고리 스코어 계산 (0-1)
        
        Score = (키워드 매칭 점수 * 0.7) + (구조 지표 점수 * 0.3)
        """
        keywords = config.get('keywords', [])
        structural_indicators = config.get('structural_indicators', [])
        
        # 키워드 매칭 점수
        keyword_score = self._calculate_keyword_score(text, keywords)
        
        # 구조 지표 매칭 점수
        structural_score = self._calculate_structural_score(text, structural_indicators)
        
        # 가중 합산
        total_score = (keyword_score * 0.7) + (structural_score * 0.3)
        
        return min(total_score, 1.0)
    
    def _calculate_keyword_score(self, text: str, keywords: List[str]) -> float:
        """
        키워드 매칭 점수 계산
        
        - 매칭 키워드 개수가 많을수록 높은 점수
        - 텍스트 길이 정규화
        """
        if not keywords:
            return 0.0
        
        match_count = 0
        for keyword in keywords:
            pattern = r'\b' + re.escape(keyword) + r'\b'
            matches = len(re.findall(pattern, text, re.IGNORECASE))
            match_count += matches
        
        # 정규화: 키워드 25%가 매칭되면 최고 점수
        max_expected_matches = len(keywords) * 0.25 * 3  # 평균 3회 매칭 가정
        score = min(match_count / max(max_expected_matches, 1), 1.0)
        
        return score
    
    def _calculate_structural_score(self, text: str, indicators: List[str]) -> float:
        """
        구조 지표 매칭 점수 계산
        
        - 정규식 기반 도메인 구조 분석
        """
        if not indicators:
            return 0.0
        
        match_count = 0
        for indicator in indicators:
            # 각 지표를 정규식으로 변환
            pattern = indicator.replace('.*', r'.*?')
            matches = len(re.findall(pattern, text, re.IGNORECASE))
            match_count += matches
        
        # 정규화
        max_expected_matches = len(indicators) * 1.5
        score = min(match_count / max(max_expected_matches, 1), 1.0)
        
        return score
    
    def _generate_reasoning(self, primary: str, primary_score: float,
                           secondary: str = None, secondary_score: float = 0.0) -> str:
        """분류 이유 생성"""
        reasons = []
        
        if primary_score >= 0.8:
            confidence_level = "매우 높음"
        elif primary_score >= 0.6:
            confidence_level = "높음"
        elif primary_score >= 0.4:
            confidence_level = "중간"
        else:
            confidence_level = "낮음"
        
        reasons.append(f"{primary} (신뢰도: {confidence_level})")
        
        if secondary and secondary_score >= 0.4:
            reasons.append(f"부분적으로 {secondary} 특성 포함")
        
        return ", ".join(reasons)
    
    def get_category_description(self, category: str) -> str:
        """카테고리 설명"""
        descriptions = {
            'marketplace': '온라인 마켓플레이스 (상품 판매/거래)',
            'forum': '온라인 포럼/토론 커뮤니티',
            'social_network': '소셜 네트워크 (SNS)',
            'communication': '통신/메시지 플랫폼',
            'blog': '일반 블로그',
            'news': '뉴스/미디어 사이트',
            'personal_blog': '개인 블로그',
            'product_promotion': '제품/서비스 홍보 사이트',
            'documentation': '기술 문서/위키',
            'marketplace_forum_mixed': '마켓 + 포럼 혼합',
            'authentication_required': '로그인 필수 (접근 제한)',
            'unknown': '분류 불가능'
        }
        
        return descriptions.get(category, '알 수 없음')
