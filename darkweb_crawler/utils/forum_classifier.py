"""포럼 자동 분류기 - 도메인의 실제 용도 판단"""

import json
import re
from pathlib import Path
from utils.logger import get_logger

logger = get_logger(__name__)

class ForumClassifier:
    """도메인이 포럼인지, 어느 카테고리인지 자동 분류"""
    
    def __init__(self):
        self.config = self._load_config()
        self.known_domains_map = self._build_known_domains_map()
    
    def _load_config(self) -> dict:
        """forums.json 로드 (상대 경로로 찾음)"""
        # 현재 파일 위치에서 시작해서 forums.json 찾기
        # utils/forum_classifier.py → ../config/forums.json
        current_dir = Path(__file__).parent
        config_path = current_dir.parent / 'config' / 'forums.json'
        
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception as e:
            logger.error(f"forums.json 로드 실패 ({config_path}): {str(e)}")
            return {}
    
    def _build_known_domains_map(self) -> dict:
        """알려진 도메인을 빠르게 찾기 위한 맵 생성
        
        Returns:
            {domain: {name, category, is_forum, ...}}
        """
        domains_map = {}
        
        for forum in self.config.get('target_forums', []):
            category = forum.get('category')
            category_info = self.config.get('category_mapping', {}).get(category, {})
            
            for domain in forum.get('known_domains', []):
                domains_map[domain.lower()] = {
                    'name': forum.get('name'),
                    'category': category,
                    'category_name': category_info.get('name', category),
                    'is_forum': category_info.get('is_forum', False),
                    'sociability': category_info.get('sociability', 'low'),
                    'type': forum.get('type', 'unknown'),
                    'source': 'known_database'
                }
        
        return domains_map
    
    def classify_domain(self, domain: str, html_content: str = None) -> dict:
        """도메인 분류 (방법 3: 알려진 리스트 먼저, 없으면 컨텐츠 분석)
        
        Args:
            domain: 도메인 이름
            html_content: 도메인의 HTML 컨텐츠 (선택)
        
        Returns:
            {
                'domain': domain,
                'is_forum': bool,
                'category': category_name,
                'category_code': category,
                'confidence': float (0-1),
                'reason': str,
                'source': 'known_database' or 'content_analysis'
            }
        """
        
        domain_lower = domain.lower()
        
        # [방법 3-1] 알려진 도메인 확인
        if domain_lower in self.known_domains_map:
            known_info = self.known_domains_map[domain_lower]
            return {
                'domain': domain,
                'is_forum': known_info['is_forum'],
                'category': known_info['category_name'],
                'category_code': known_info['category'],
                'forum_name': known_info['name'],
                'confidence': 1.0,
                'reason': f"알려진 포럼: {known_info['name']}",
                'source': 'known_database',
                'sociability': known_info['sociability'],
                'type': known_info['type']
            }
        
        # 부분 일치 확인 (예: dreadytofvwu4oa6.onion vs dreadytofvwu4oa67.onion)
        for known_domain, info in self.known_domains_map.items():
            # 앞의 일부분이 일치하고 .onion으로 끝나면 같은 포럼으로 봄
            if domain_lower.endswith('.onion') and known_domain.endswith('.onion'):
                domain_prefix = domain_lower.split('.onion')[0]
                known_prefix = known_domain.split('.onion')[0]
                
                # 5글자 이상 일치하면 동일 포럼으로 판단
                if len(domain_prefix) > 5 and len(known_prefix) > 5:
                    common_length = 0
                    for i in range(min(len(domain_prefix), len(known_prefix))):
                        if domain_prefix[i] == known_prefix[i]:
                            common_length += 1
                        else:
                            break
                    
                    if common_length >= 10:  # 10글자 이상 일치
                        return {
                            'domain': domain,
                            'is_forum': info['is_forum'],
                            'category': info['category_name'],
                            'category_code': info['category'],
                            'forum_name': info['name'],
                            'confidence': 0.95,
                            'reason': f"알려진 포럼 ({info['name']})의 미러 사이트로 추정",
                            'source': 'known_database',
                            'sociability': info['sociability'],
                            'type': info['type']
                        }
        
        # [방법 3-2] 컨텐츠 기반 분류
        if html_content:
            return self._classify_by_content(domain, html_content)
        
        # [방법 3-3] 혼합 불가 시 기본값
        return {
            'domain': domain,
            'is_forum': None,
            'category': 'unknown',
            'category_code': 'unknown',
            'confidence': 0.0,
            'reason': '알려진 포럼 정보 없음, HTML 컨텐츠도 제공되지 않음',
            'source': 'unknown',
            'sociability': 'low',
            'type': 'unknown'
        }
    
    def _classify_by_content(self, domain: str, html_content: str) -> dict:
        """HTML 컨텐츠 기반 포럼 분류"""
        
        content_lower = html_content.lower()
        keywords = self.config.get('forum_keywords', {})
        
        # 키워드 매칭 점수 계산
        scores = {
            'discussion': self._count_keywords(content_lower, keywords.get('discussion_indicators', [])),
            'marketplace': self._count_keywords(content_lower, keywords.get('marketplace_indicators', [])),
            'archive': self._count_keywords(content_lower, keywords.get('archive_indicators', []))
        }
        
        max_score = max(scores.values()) if scores else 0
        
        if max_score == 0:
            return {
                'domain': domain,
                'is_forum': False,
                'category': 'unknown',
                'category_code': 'unknown',
                'confidence': 0.0,
                'reason': '포럼 특성을 나타내는 키워드 없음',
                'source': 'content_analysis',
                'sociability': 'low',
                'type': 'unknown'
            }
        
        # 최고 점수 카테고리 판단
        if scores['discussion'] >= scores['marketplace'] and scores['discussion'] >= scores['archive']:
            category_code = 'discussion_forum'
            category_name = self.config.get('category_mapping', {}).get(category_code, {}).get('name', 'Discussion Forum')
            is_forum = True
            confidence = min(scores['discussion'] / 100, 1.0)
        elif scores['marketplace'] >= scores['archive']:
            category_code = 'marketplace'
            category_name = self.config.get('category_mapping', {}).get(category_code, {}).get('name', 'Marketplace')
            is_forum = False
            confidence = min(scores['marketplace'] / 100, 1.0)
        else:
            category_code = 'archive'
            category_name = self.config.get('category_mapping', {}).get(category_code, {}).get('name', 'Archive')
            is_forum = False
            confidence = min(scores['archive'] / 80, 1.0)  # archive는 점수가 적을 수 있음
        
        return {
            'domain': domain,
            'is_forum': is_forum,
            'category': category_name,
            'category_code': category_code,
            'confidence': round(confidence, 2),
            'reason': f"컨텐츠 분석 (Discussion: {scores['discussion']}, Marketplace: {scores['marketplace']}, Archive: {scores['archive']})",
            'source': 'content_analysis',
            'sociability': self.config.get('category_mapping', {}).get(category_code, {}).get('sociability', 'low'),
            'type': 'detected'
        }
    
    def _count_keywords(self, text: str, keywords: list) -> int:
        """텍스트에서 키워드 개수 세기"""
        count = 0
        for keyword in keywords:
            # 단어 경계를 포함한 정규표현식으로 검사
            pattern = r'\b' + re.escape(keyword) + r'\b'
            count += len(re.findall(pattern, text, re.IGNORECASE))
        return count
    
    def get_sociable_forums(self, classification_dict: dict) -> bool:
        """분류 결과에서 '소통 가능한 포럼'인지 판단
        
        Returns:
            True if 포럼이면서 sociability가 'high' 또는 'medium'
        """
        return (
            classification_dict.get('is_forum') and 
            classification_dict.get('sociability') in ['high', 'medium']
        )
    
    def get_all_forum_categories(self) -> dict:
        """모든 포럼 카테고리 반환"""
        return self.config.get('category_mapping', {})
    
    def is_forum(self, domain: str, html_content: str = None) -> bool:
        """도메인이 포럼인지 판단"""
        result = self.classify_domain(domain, html_content)
        return result.get('is_forum', False)
    
    def get_category(self, domain: str, html_content: str = None) -> str:
        """도메인의 카테고리 반환"""
        result = self.classify_domain(domain, html_content)
        return result.get('category', 'Unknown')


if __name__ == '__main__':
    # 테스트
    classifier = ForumClassifier()
    
    # 알려진 도메인 테스트
    test_domains = [
        'dreadytofvwu4oa6.onion',
        'xsshq4w3f35dv7q7.onion',
        'unknown123456789.onion'
    ]
    
    print("=" * 80)
    print("포럼 분류기 테스트")
    print("=" * 80)
    
    for domain in test_domains:
        result = classifier.classify_domain(domain)
        print(f"\n도메인: {domain}")
        print(f"  포럼 여부: {result.get('is_forum')}")
        print(f"  카테고리: {result.get('category')}")
        print(f"  신뢰도: {result.get('confidence')}")
        print(f"  사유: {result.get('reason')}")
        print(f"  출처: {result.get('source')}")
    
    # 모든 카테고리 출력
    print("\n" + "=" * 80)
    print("사용 가능한 카테고리")
    print("=" * 80)
    for code, info in classifier.get_all_forum_categories().items():
        print(f"  {code}: {info.get('name')} (포럼: {info.get('is_forum')}, 소통성: {info.get('sociability')})")
