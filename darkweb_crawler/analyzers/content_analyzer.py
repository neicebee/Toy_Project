"""
HTML 콘텐츠 분석기 - 불법 키워드 탐지 및 사이트 목적성 분석
"""

import re
import logging
from typing import Dict, List, Tuple
from collections import Counter

logger = logging.getLogger(__name__)


class ContentAnalyzer:
    """HTML 콘텐츠를 분석하여 불법성 판정"""
    
    def __init__(self, keywords_config: Dict = None):
        """
        Args:
            keywords_config: 불법 키워드 분류 사전
        """
        self.keywords_config = keywords_config or self._default_keywords()
        self.html_cleaners = [
            r'<script[^>]*>.*?</script>',  # 스크립트 제거
            r'<style[^>]*>.*?</style>',    # 스타일 제거
            r'<[^>]+>',                     # HTML 태그 제거
        ]
    
    def _default_keywords(self) -> Dict:
        """기본 불법 키워드 설정"""
        return {
            'dark_market': [
                'marketplace', 'market', 'shop', 'store', 'buyer', 'seller',
                'product', 'price', 'deal', 'transaction', 'payment',
                'btc', 'bitcoin', 'cryptocurrency', 'escrow', 'vendor'
            ],
            'drugs_narcotics': [
                'cocaine', 'heroin', 'methamphetamine', 'fentanyl', 'mdma',
                'lsd', 'cannabis', 'marijuana', 'drug', 'narcotic', 'poison',
                '약물', '마약', '약', '코카인', '헤로인', '메스암페타민'
            ],
            'weapons_explosives': [
                'firearms', 'gun', 'rifle', 'pistol', 'ammunition', 'explosives',
                'bomb', 'grenade', 'weapon', 'arms', '총', '화기', '폭탄'
            ],
            'hacking_carding': [
                'hack', 'hacking', 'exploit', 'vulnerability', 'carding',
                'credit card', 'cvv', 'dump', 'fullz', 'ssn', 'dox',
                '해킹', '카딩', '크레딧카드', 'ssn'
            ],
            'illegal_content': [
                'cp', 'child', 'adult', 'porn', 'xxx', 'sex', 'escort',
                'prostitution', 'abuse', '아동', '음란', '성인'
            ],
            'fraud_scam': [
                'fraud', 'scam', 'fake', 'stolen', 'counterfeit', 'forgery',
                'phishing', 'money laundering', '사기', '위조', '사칭'
            ],
            'forum_indicators': [
                'forum', 'discussion', 'thread', 'post', 'member', 'user',
                'comment', 'reply', 'topic', 'board', 'chat', 'community',
                'community', '포럼', '토론', '커뮤니티'
            ]
        }
    
    def clean_html(self, html: str) -> str:
        """HTML 정제 - 태그 및 스크립트 제거"""
        if not html:
            return ""
        
        text = html
        
        # 정규식으로 제거
        for pattern in self.html_cleaners:
            text = re.sub(pattern, ' ', text, flags=re.IGNORECASE | re.DOTALL)
        
        # 여러 공백을 하나로
        text = re.sub(r'\s+', ' ', text)
        text = text.strip()
        
        return text
    
    def extract_keywords(self, text: str, category: str) -> Tuple[List[str], int]:
        """
        텍스트에서 특정 카테고리의 키워드 추출
        
        Args:
            text: 분석할 텍스트
            category: 키워드 카테고리
        
        Returns:
            (발견된 키워드 리스트, 매칭 횟수)
        """
        if category not in self.keywords_config:
            return [], 0
        
        keywords = self.keywords_config[category]
        found_keywords = []
        match_count = 0
        
        text_lower = text.lower()
        
        for keyword in keywords:
            # 단어 경계를 포함한 정규식 검색 (더 정확함)
            pattern = r'\b' + re.escape(keyword) + r'\b'
            matches = re.findall(pattern, text_lower, re.IGNORECASE)
            
            if matches:
                found_keywords.append(keyword)
                match_count += len(matches)
        
        return found_keywords, match_count
    
    def detect_illegal_content(self, html: str, verbose: bool = False) -> Dict:
        """
        불법 콘텐츠 탐지
        
        Args:
            html: 분석할 HTML
            verbose: 상세 정보 출력 여부
        
        Returns:
            {
                'is_illegal': bool,
                'illegal_confidence': float (0-1),
                'categories': {
                    'category_name': {
                        'found': bool,
                        'keywords': ['keyword1', ...],
                        'match_count': int,
                        'severity': 'high' | 'medium' | 'low'
                    }
                },
                'primary_illegal_type': str or None,
                'total_matches': int
            }
        """
        text = self.clean_html(html)
        
        if not text:
            logger.warning("분석할 텍스트가 없음")
            return {
                'is_illegal': False,
                'illegal_confidence': 0.0,
                'categories': {},
                'primary_illegal_type': None,
                'total_matches': 0,
                'error': 'No text to analyze'
            }
        
        category_results = {}
        total_matches = 0
        illegal_categories = []
        
        # 각 카테고리별 분석
        for category in self.keywords_config.keys():
            keywords, match_count = self.extract_keywords(text, category)
            
            if keywords:
                severity = self._calculate_severity(category, match_count, len(text))
                
                category_results[category] = {
                    'found': True,
                    'keywords': keywords,
                    'match_count': match_count,
                    'severity': severity
                }
                
                # 심각도가 높은 카테고리만 불법으로 판정
                if severity in ['high', 'medium']:
                    illegal_categories.append((category, severity, match_count))
                
                total_matches += match_count
                
                if verbose:
                    logger.info(f"  [{category}] 발견: {len(keywords)}개 키워드, "
                              f"{match_count}회 매칭, 심각도={severity}")
            else:
                category_results[category] = {
                    'found': False,
                    'keywords': [],
                    'match_count': 0,
                    'severity': 'none'
                }
        
        # 불법성 판정
        is_illegal = len(illegal_categories) > 0
        
        # 주요 불법 카테고리 선정 (심각도 + 매칭 횟수)
        primary_illegal_type = None
        if illegal_categories:
            # 심각도 높음 우선, 그 다음 매칭 횟수 많은 순
            severity_order = {'high': 0, 'medium': 1, 'low': 2}
            illegal_categories.sort(
                key=lambda x: (severity_order[x[1]], -x[2])
            )
            primary_illegal_type = illegal_categories[0][0]
        
        # 신뢰도 계산 (0-1 범위)
        confidence = self._calculate_illegal_confidence(
            total_matches,
            len(text),
            len(illegal_categories)
        )
        
        result = {
            'is_illegal': is_illegal,
            'illegal_confidence': confidence,
            'categories': category_results,
            'primary_illegal_type': primary_illegal_type,
            'total_matches': total_matches
        }
        
        if verbose:
            logger.info(f"불법 콘텐츠 판정: 불법={is_illegal}, "
                       f"신뢰도={confidence:.2f}, "
                       f"주요 카테고리={primary_illegal_type}")
        
        return result
    
    def _calculate_severity(self, category: str, match_count: int, text_length: int) -> str:
        """
        매칭 횟수 기반 심각도 계산
        
        Returns:
            'high' | 'medium' | 'low'
        """
        # 텍스트 대비 매칭 비율
        match_ratio = match_count / max(text_length, 1)
        
        # 카테고리별 기본 심각도
        high_severity_categories = [
            'drugs_narcotics', 'weapons_explosives', 'illegal_content'
        ]
        medium_severity_categories = [
            'hacking_carding', 'fraud_scam', 'dark_market'
        ]
        
        # 기본 심각도 설정
        if category in high_severity_categories:
            base_severity = 'high'
        elif category in medium_severity_categories:
            base_severity = 'medium'
        else:
            base_severity = 'low'
        
        # 비율에 따라 조정
        if match_count >= 10 or match_ratio > 0.05:  # 높은 빈도
            return 'high'
        elif match_count >= 3 or match_ratio > 0.01:  # 중간 빈도
            return base_severity
        else:  # 낮은 빈도
            if base_severity == 'high':
                return 'medium'
            return 'low'
    
    def _calculate_illegal_confidence(self, total_matches: int, text_length: int, 
                                     category_count: int) -> float:
        """
        불법 신뢰도 계산 (0-1)
        
        고려 요소:
        - 총 매칭 횟수
        - 텍스트 길이 대비 비율
        - 불법 카테고리 개수
        """
        if total_matches == 0:
            return 0.0
        
        # 비율 기반 점수 (0-0.5)
        match_ratio = min(total_matches / max(text_length, 1), 1.0)
        ratio_score = match_ratio * 50  # 0-50점
        
        # 카테고리 개수 기반 점수 (0-0.3)
        category_score = min(category_count * 15, 30)  # 0-30점
        
        # 총점 합산 (0-80)
        confidence = (ratio_score + category_score) / 100.0
        
        # 0-1 범위로 정규화
        confidence = min(confidence, 1.0)
        
        return confidence
