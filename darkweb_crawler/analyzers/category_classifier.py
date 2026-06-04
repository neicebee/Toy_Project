"""
사이트 카테고리 분류기 - HTML 콘텐츠 기반 사이트 목적성 분류
"""

import re
import logging
import importlib
from typing import Dict, List

from utils.html_cleaner import HTMLCleaner

logger = logging.getLogger(__name__)


class CategoryClassifier:
    """HTML 콘텐츠를 분석하여 사이트 카테고리 분류"""
    
    def __init__(self, categories_config: Dict = None, use_bart: bool = True,
                 bart_model_name: str = 'facebook/bart-large-mnli',
                 rule_weight: float = 0.65, bart_weight: float = 0.35):
        """
        Args:
            categories_config: 카테고리별 특성 사전
            use_bart: BART zero-shot 분류 사용 여부
            bart_model_name: 사용할 BART/zero-shot 모델 이름
            rule_weight: 규칙 기반 점수 가중치
            bart_weight: BART 점수 가중치
        """
        self.categories_config = categories_config or self._default_categories()
        self.use_bart = use_bart
        self.bart_model_name = bart_model_name
        self.rule_weight = rule_weight
        self.bart_weight = bart_weight
        self._bart_classifier = None
        self._bart_loaded = False
        self.bart_chunk_size = 400
        self.bart_max_chunks = 12
        self._validate_weights()

    def _validate_weights(self):
        total = self.rule_weight + self.bart_weight
        if total <= 0:
            self.rule_weight = 1.0
            self.bart_weight = 0.0
            return

        self.rule_weight = self.rule_weight / total
        self.bart_weight = self.bart_weight / total

    def _load_bart_classifier(self):
        """BART zero-shot 분류기 lazy loading"""
        if self._bart_loaded:
            return self._bart_classifier

        self._bart_loaded = True
        logger.info("🔄 BART 분류기 로드 시작...")

        if not self.use_bart:
            logger.info("BART 분류 비활성화")
            return None

        try:
            transformers_module = importlib.import_module('transformers')
            pipeline = transformers_module.pipeline

            logger.info("🔄 BART 모델 로딩 중...")
            
            self._bart_classifier = pipeline(
                'zero-shot-classification',
                model=self.bart_model_name,
                tokenizer=self.bart_model_name,
                device=-1
            )
            logger.info(f"✅ BART 분류기 로드 완료")
        except Exception as e:
            self._bart_classifier = None
            logger.error(f"❌ BART 분류기 로드 실패: {str(e)}")
            logger.warning(f"   규칙 기반만 사용합니다")

        return self._bart_classifier
    
    def _default_categories(self) -> Dict:
        """기본 카테고리 설정"""
        return {
            'marketplace': {
                'keywords': [
                    'marketplace', 'market', 'shop', 'store', 'vendor', 'seller', 'buyer',
                    'product', 'price', 'cost', 'payment', 'sale', 'buy', 'sell',
                    'item', 'inventory', 'listing', 'transaction', 'deal',
                    'cart', 'checkout', 'order', 'delivery', 'shipment',
                    'bid', 'auction', 'escrow', 'merchant', 'supplier', 'wholesale',
                    'fulfillment', 'dropship', 'ecommerce', 'commerce', 'trade',
                    'review', 'rating', 'feedback', 'reputation',
                    '마켓', '상점', '판매', '구매', '거래', '판매자', '가격'
                ],
                'structural_indicators': [
                    'product.*page', 'product.*detail', 'cart', 'checkout', 'vendor.*profile',
                    'seller.*profile', 'search.*product', 'category.*product',
                    'add.*cart', 'quantity', 'buy.*now', 'add.*to.*cart',
                    'shipping.*info', 'shipping.*address', 'payment.*method',
                    'product.*review', 'rating', 'feedback', 'review.*section'
                ]
            },
            'social_communication': {
                'keywords': [
                    'social', 'network', 'profile', 'friend', 'follower', 'follow',
                    'feed', 'timeline', 'post', 'share', 'like', 'comment', 'user',
                    'account', 'community', 'group', 'page', 'wall',
                    'notification', 'unfollow', 'retweet', 'favorite', 'unfriend',
                    'photo', 'video', 'story', 'status', 'mood', 'emoji',
                    'communication', 'contact', 'email', 'message', 'chat', 'messenger',
                    'direct', 'dm', 'mail', 'send', 'receive', 'letter', 'envelope',
                    'talk', 'speak', 'conversation', 'inbox', 'thread',
                    '소셜', '프로필', '팔로우', '팔로워', '게시물',
                    '통신', '메시지', '채팅', '대화', '친구'
                ],
                'structural_indicators': [
                    'profile.*page', 'profile.*picture', 'profile.*info',
                    'user.*feed', 'timeline', 'friend.*request', 'friend.*list',
                    'message.*inbox', 'send.*message', 'contact.*form',
                    'direct.*message', 'chat.*interface', 'chat.*room', 'messenger',
                    'notification.*bell', 'notification.*center', 'notification.*icon',
                    'post.*form', 'compose.*message', 'reply.*button'
                ]
            },
            'blog': {
                'keywords': [
                    'blog', 'post', 'article', 'author', 'publish', 'date', 'time',
                    'category', 'tag', 'comment', 'archive', 'recent post',
                    'about', 'contact', 'subscribe', 'rss', 'feed',
                    'permalink', 'sidebar', 'widget', 'pagination', 'excerpt',
                    'teaser', 'entry', 'blogger', 'blogger profile',
                    'posted.*by', 'published', 'updated', 'comments.*on',
                    '블로그', '포스팅', '글', '기사', '작가', '댓글'
                ],
                'structural_indicators': [
                    'blog.*post', 'article.*content', 'post.*date', 'post.*header',
                    'sidebar.*recent', 'archive', 'category.*list',
                    'post.*footer', 'comment.*section', 'comment.*form',
                    'tags', 'related.*post', 'previous.*next'
                ]
            },
            'news': {
                'keywords': [
                    'news', 'article', 'breaking', 'headline', 'update', 'report',
                    'journalist', 'press', 'media', 'publication', 'story',
                    'date', 'time', 'published', 'category', 'section',
                    'byline', 'dateline', 'correspondent', 'editor', 'columnist',
                    'opinion', 'editorial', 'press.*release', 'wire.*service',
                    'news.*feed', 'latest.*news', 'top.*stories',
                    '뉴스', '기사', '보도', '기자', '뉴스레터'
                ],
               'structural_indicators': [
                    'news.*section', 'headline', 'article.*link', 'publish.*date',
                    'breaking.*news', 'latest.*news', 'top.*stories',
                    'news.*item', 'story.*card', 'article.*header',
                    'byline', 'journalist.*info', 'related.*stories',
                    'news.*feed'
                ]
            },
            'product_promotion': {
                'keywords': [
                    'product', 'service', 'software', 'tool', 'application', 'app',
                    'feature', 'benefit', 'price', 'plan', 'buy', 'download',
                    'free', 'trial', 'license', 'subscription', 'version',
                    'spec', 'specification', 'compare', 'review', 'rating',
                    'testimonial', 'case.*study', 'whitepaper', 'datasheet',
                    'demo', 'screenshot', 'video.*demo', 'tour',
                    'pricing.*model', 'tier', 'enterprise', 'startup',
                    '제품', '서비스', '구독', '기능', '가격'
                ],
                'structural_indicators': [
                    'product.*feature', 'feature.*list', 'pricing.*plan',
                    'pricing.*table', 'download.*link', 'get.*started',
                    'download.*button', 'try.*free', 'free.*trial',
                    'compare.*plan', 'spec.*sheet', 'datasheet',
                    'testimonial', 'customer.*quote', 'case.*study'
                ]
            },
            'documentation': {
                'keywords': [
                    'documentation', 'guide', 'tutorial', 'manual', 'help', 'faq',
                    'instruction', 'api', 'reference', 'developer', 'code',
                    'example', 'version', 'changelog', 'readme', 'wiki',
                    'install', 'deployment', 'configuration', 'setup',
                    'troubleshoot', 'debug', 'error', 'issue', 'problem',
                    'release.*note', 'version.*history', 'api.*doc',
                    'class', 'method', 'parameter', 'return.*value',
                    '문서', '가이드', 'API', '튜토리얼', '설치'
                ],
                'structural_indicators': [
                    'api.*reference', 'api.*doc', 'code.*example', 'getting.*started',
                    'faq', 'help.*section', 'wiki', 'documentation.*index',
                    'install.*guide', 'deployment.*guide', 'troubleshoot.*guide',
                    'function.*reference', 'class.*reference', 'method.*list',
                    'code.*sample', 'tutorial.*section', 'quick.*start'
                ]
            },
            'authentication_required': {
                # 로그인 페이지 (인증 필요)
                'keywords': [
                    'login', 'sign in', 'sign up', 'register', 'password', 'username',
                    'email', 'authentication', 'auth', 'member', 'account',
                    'authorize', 'credential', 'user', 'access', 'restricted',
                    'signin', 'signup', 'logon', 'log on', 'log in',
                    'authenticate', 'verify', 'confirm', 'private', 'members.*only',
                    'admin.*login', 'admin.*access', 'session', 'token',
                    'two.*factor', '2fa', 'otp', 'verify.*identity',
                    '로그인', '비밀번호', '회원가입', '사용자명', '인증', '로그온'
                ],
                'structural_indicators': [
                    'login.*form', 'login.*page', 'password.*field', 'username.*field',
                    'email.*field', 'submit.*button', 'remember.*me', 'forgot.*password',
                    'login.*box', 'credentials', 'authenticate', 'verify.*identity',
                    'access.*denied', 'restricted.*access', 'members.*only',
                    'password.*reset', 'password.*recovery', 'session.*expired'
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
        text = HTMLCleaner.clean(html).lower()
        
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
        rule_scores_dict = {}  # 규칙 기반 점수 저장 (재계산 방지)
        bart_scores = self._calculate_bart_scores(text)
        
        # 각 카테고리별 스코어 계산
        for category, config in self.categories_config.items():
            rule_score = self._calculate_category_score(text, config)
            rule_scores_dict[category] = rule_score  # 규칙 점수 저장
            bart_score = bart_scores.get(category, 0.0)
            score = (rule_score * self.rule_weight) + (bart_score * self.bart_weight)
            category_scores[category] = min(score, 1.0)
            
            if verbose and score > 0:
                logger.info(f"  [{category}] 규칙={rule_score:.2f}, BART={bart_score:.2f}, 종합={score:.2f}")
        
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
            'reasoning': reasoning,
            'rule_scores': rule_scores_dict,  # 반복문에서 미리 계산한 값 사용 (성능 개선)
            'bart_scores': bart_scores,
            'bart_model_used': self._bart_classifier is not None
        }
        
        if verbose:
            logger.info(f"분류 결과: {primary_category} (신뢰도: {confidence:.2f})")
        
        return result

    def _calculate_bart_scores(self, text: str) -> Dict[str, float]:
        """BART zero-shot으로 카테고리별 점수 계산"""
        if not self.use_bart:
            return {}

        classifier = self._load_bart_classifier()
        if not classifier:
            return {}

        text_chunks = self._split_text_for_bart(text)
        if not text_chunks:
            return {}

        label_map = {
            'marketplace': 'vendor shops, product listings, trade, commerce',
            'social_communication': 'social media, messaging, chat, profiles, networking',
            'blog': 'articles, blog posts, personal writing, essays',
            'news': 'news reports, journalism, current events, headlines',
            'product_promotion': 'brand marketing, advertising, product features, showcases',
            'documentation': 'technical manuals, API guides, reference docs, wikis',
            'authentication_required': 'login page, password gate, registration form, restricted access'
        }

        candidate_labels = [label_map[k] for k in self.categories_config.keys() if k in label_map]
        reverse_label = {v: k for k, v in label_map.items()}
        score_buckets = {category: [] for category in self.categories_config.keys()}

        try:
            for chunk in text_chunks:
                result = classifier(
                    chunk,
                    candidate_labels,
                    multi_label=True,
                    hypothesis_template='This page is about {}.',
                    max_length=512,  # BART 토큰 길이 제한
                    truncation=True  # 초과 토큰 자동 절단
                )

                for label, score in zip(result.get('labels', []), result.get('scores', [])):
                    category = reverse_label.get(label)
                    if category:
                        score_buckets[category].append(float(score))

            bart_scores = {}
            for category, values in score_buckets.items():
                if not values:
                    bart_scores[category] = 0.0
                    continue

                max_score = max(values)
                mean_score = sum(values) / len(values)
                bart_scores[category] = min((max_score * 0.7) + (mean_score * 0.3), 1.0)

            return bart_scores

        except Exception as e:
            logger.warning(f"BART 분류 실패 - 규칙 기반만 사용: {str(e)}")
            return {}

    def _split_text_for_bart(self, text: str) -> List[str]:
        """문장 경계 기반으로 BART 입력 청크 생성"""
        if not text:
            return []

        sentences = re.split(r'(?<=[.!?。！？])\s+|\n+', text)
        sentences = [s.strip() for s in sentences if s and s.strip()]

        if not sentences:
            trimmed = text[:self.bart_chunk_size].strip()
            return [trimmed] if trimmed else []

        chunks = []
        current_chunk = []
        current_len = 0

        for sentence in sentences:
            sentence_len = len(sentence)

            if sentence_len > self.bart_chunk_size:
                if current_chunk:
                    chunks.append(' '.join(current_chunk))
                    current_chunk = []
                    current_len = 0

                for i in range(0, sentence_len, self.bart_chunk_size):
                    piece = sentence[i:i + self.bart_chunk_size].strip()
                    if piece:
                        chunks.append(piece)
                continue

            if current_len + sentence_len + 1 <= self.bart_chunk_size:
                current_chunk.append(sentence)
                current_len += sentence_len + 1
            else:
                if current_chunk:
                    chunks.append(' '.join(current_chunk))
                current_chunk = [sentence]
                current_len = sentence_len

        if current_chunk:
            chunks.append(' '.join(current_chunk))

        # 모든 청크 반환 (이전의 12개 제한 제거) → 더 정확한 분석
        return chunks
    
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
            
            if any(kw in meta for kw in ['marketplace', 'trading', 'vendor', 'product', '마켓']):
                return 'marketplace'
            elif any(kw in meta for kw in ['forum', 'community', 'discussion', '포럼']):
                return 'forum'
            elif any(kw in meta for kw in ['social', 'social network', 'sns']):
                return 'social_network'
        
        # 페이지 본문의 명시적 설명 텍스트에서 추론
        # (로그인 폼 근처에 있을 가능성 있음)
        text = HTMLCleaner.clean(html).lower()
        
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
