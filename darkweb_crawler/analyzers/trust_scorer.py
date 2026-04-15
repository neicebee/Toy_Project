"""
신뢰도 점수 계산기 - 멀티 팩터 신뢰도 평가 시스템
"""

import logging
from typing import Dict

logger = logging.getLogger(__name__)


class TrustScorer:
    """
    멀티 팩터 신뢰도 점수 계산
    
    총 신뢰도 = 접근성(40) + 색인(30) + 콘텐츠(30)
    """
    
    def __init__(self):
        """신뢰도 점수 계산기 초기화"""
        self.weights = {
            'accessibility': 40,
            'indexing': 30,
            'content': 30
        }
    
    def calculate_accessibility_score(self, status_code: int = None, 
                                     is_accessible: bool = False,
                                     redirect_domain: str = None) -> int:
        """
        접근성 점수 계산 (40점 만점)
        
        Args:
            status_code: HTTP 상태 코드
            is_accessible: 접근 가능 여부
            redirect_domain: 리다이렉트 대상 도메인
        
        Returns:
            0-40 점수
        """
        # 상태코드 200: 정상 접근 (40점)
        if status_code == 200 and is_accessible:
            logger.debug("✅ 접근성: 200 OK (40점)")
            return 40
        
        # 상태코드 300번대 + 리다이렉트 성공: 부분 신뢰 (30점)
        if 300 <= status_code < 400:
            if redirect_domain:
                logger.debug(f"⚠️ 접근성: {status_code} 리다이렉트 → {redirect_domain} (30점)")
                return 30
            else:
                logger.debug(f"⚠️ 접근성: {status_code} 리다이렉트 불가 (10점)")
                return 10
        
        # 상태코드 400, 500: 클라이언트/서버 에러 (0점)
        if status_code and 400 <= status_code < 600:
            logger.debug(f"❌ 접근성: {status_code} 에러 (0점)")
            return 0
        
        # 접근 불가: 위험도 계산 필요 (색인 정보로 보정)
        if not is_accessible:
            logger.debug("❌ 접근성: 접근 불가 (0점, 색인으로 보정 가능)")
            return 0
        
        # 기타
        logger.debug("❓ 접근성: 미분류 (10점)")
        return 10
    
    def calculate_indexing_score(self, ahmia_found: bool = False,
                                duckduckgo_found: bool = False,
                                combined_found: bool = False) -> int:
        """
        색인 신뢰도 점수 계산 (30점 만점)
        
        Args:
            ahmia_found: Ahmia에서 검색됨
            duckduckgo_found: DuckDuckGo에서 검색됨
            combined_found: Ahmia 또는 DuckDuckGo에서 검색됨
        
        Returns:
            0-30 점수
        """
        found_count = sum([ahmia_found, duckduckgo_found])
        
        # 양쪽 다 검색됨 (30점) - 투명성 높음
        if ahmia_found and duckduckgo_found:
            logger.debug("✅ 색인: Ahmia + DuckDuckGo 모두 검색 (30점)")
            return 30
        
        # 하나만 검색됨 (15점) - 부분 투명성
        if found_count == 1:
            engine = "Ahmia" if ahmia_found else "DuckDuckGo"
            logger.debug(f"⚠️ 색인: {engine}만 검색 (15점)")
            return 15
        
        # 둘 다 검색 안됨 (0점) - 은닉됨
        logger.debug("❌ 색인: 검색 엔진에서 미발견 (0점)")
        return 0
    
    def calculate_content_score(self, is_illegal: bool = False,
                               illegal_confidence: float = 0.0,
                               html_collected: bool = False) -> int:
        """
        콘텐츠 신뢰도 점수 계산 (30점 만점)
        
        Args:
            is_illegal: 불법 콘텐츠 판정
            illegal_confidence: 불법성 신뢰도 (0-1)
            html_collected: HTML 수집 성공
        
        Returns:
            0-30 점수
        """
        # HTML 미수집: 분석 불가 (0점)
        if not html_collected:
            logger.debug("❓ 콘텐츠: HTML 미수집 (0점)")
            return 0
        
        # 불법이 아님 + 명확한 분류 가능: 높은 신뢰도 (30점)
        if not is_illegal:
            logger.debug("✅ 콘텐츠: 불법 미탐지 (30점)")
            return 30
        
        # 불법 의심 (의심 키워드 1-2개): 중간 신뢰도 (15점)
        if illegal_confidence < 0.5:
            logger.debug(f"⚠️ 콘텐츠: 약한 불법 신호 (15점, 신뢰도={illegal_confidence:.2f})")
            return 15
        
        # 불법 확실 (의심 키워드 다수): 낮은 신뢰도 (0점)
        logger.debug(f"❌ 콘텐츠: 강한 불법 신호 (0점, 신뢰도={illegal_confidence:.2f})")
        return 0
    
    def calculate_total_score(self, accessibility_score: int = 0,
                             indexing_score: int = 0,
                             content_score: int = 0) -> int:
        """
        총 신뢰도 점수 계산
        
        Returns:
            0-100 점수
        """
        total = accessibility_score + indexing_score + content_score
        total = max(0, min(total, 100))  # 0-100 범위 제한
        
        logger.debug(f"📊 총 신뢰도: {total}점 " +
                    f"(접근성={accessibility_score}, 색인={indexing_score}, 콘텐츠={content_score})")
        
        return total
    
    def get_trust_level(self, total_score: int) -> str:
        """
        신뢰도 점수를 레벨로 변환
        
        Returns:
            'HIGHLY_TRUSTWORTHY' | 'TRUSTWORTHY' | 'SUSPICIOUS' | 'UNTRUSTED' | 'UNREACHABLE'
        """
        if total_score >= 90:
            return 'HIGHLY_TRUSTWORTHY'
        elif total_score >= 70:
            return 'TRUSTWORTHY'
        elif total_score >= 50:
            return 'SUSPICIOUS'
        elif total_score >= 20:
            return 'UNTRUSTED'
        else:
            return 'UNREACHABLE'
    
    def get_trust_level_description(self, level: str) -> str:
        """신뢰도 레벨 설명"""
        descriptions = {
            'HIGHLY_TRUSTWORTHY': '높은 신뢰도 - 분석 권장',
            'TRUSTWORTHY': '신뢰도 있음',
            'SUSPICIOUS': '의심 - 주의 필요',
            'UNTRUSTED': '낮은 신뢰도 - 위험',
            'UNREACHABLE': '분석 불가 - 접근/데이터 부족'
        }
        return descriptions.get(level, '알 수 없음')
    
    def calculate_comprehensive_trust(self, analysis_data: Dict) -> Dict:
        """
        종합 신뢰도 분석
        
        Args:
            analysis_data: 서버에서 반환한 분석 데이터
            {
                'accessibility': {...},
                'indexing': {...},
                'html_collected': bool,
                'is_illegal': bool,
                'illegal_confidence': float,
                ...
            }
        
        Returns:
            {
                'accessibility_score': int (0-40),
                'indexing_score': int (0-30),
                'content_score': int (0-30),
                'total_score': int (0-100),
                'trust_level': str,
                'trust_description': str,
                'detailed_analysis': str
            }
        """
        # 각 항목별 점수 계산
        accessibility = analysis_data.get('accessibility', {})
        accessibility_score = self.calculate_accessibility_score(
            status_code=accessibility.get('status_code'),
            is_accessible=accessibility.get('is_accessible'),
            redirect_domain=accessibility.get('redirect_domain')
        )
        
        indexing = analysis_data.get('indexing', {})
        indexing_score = self.calculate_indexing_score(
            ahmia_found=indexing.get('ahmia_found', False),
            duckduckgo_found=indexing.get('duckduckgo_found', False),
            combined_found=indexing.get('combined_found', False)
        )
        
        content_score = self.calculate_content_score(
            is_illegal=analysis_data.get('is_illegal', False),
            illegal_confidence=analysis_data.get('illegal_confidence', 0.0),
            html_collected=analysis_data.get('html_collected', False)
        )
        
        # 총 점수
        total_score = self.calculate_total_score(
            accessibility_score=accessibility_score,
            indexing_score=indexing_score,
            content_score=content_score
        )
        
        # 신뢰도 레벨
        trust_level = self.get_trust_level(total_score)
        trust_description = self.get_trust_level_description(trust_level)
        
        # 상세 분석 텍스트
        detailed_analysis = self._generate_detailed_analysis(
            accessibility,
            indexing,
            analysis_data,
            accessibility_score,
            indexing_score,
            content_score
        )
        
        return {
            'accessibility_score': accessibility_score,
            'indexing_score': indexing_score,
            'content_score': content_score,
            'total_score': total_score,
            'trust_level': trust_level,
            'trust_description': trust_description,
            'detailed_analysis': detailed_analysis,
            'score_breakdown': {
                'accessibility': {
                    'score': accessibility_score,
                    'max': 40,
                    'percentage': round((accessibility_score / 40) * 100, 1) if accessibility_score > 0 else 0
                },
                'indexing': {
                    'score': indexing_score,
                    'max': 30,
                    'percentage': round((indexing_score / 30) * 100, 1) if indexing_score > 0 else 0
                },
                'content': {
                    'score': content_score,
                    'max': 30,
                    'percentage': round((content_score / 30) * 100, 1) if content_score > 0 else 0
                }
            }
        }
    
    def _generate_detailed_analysis(self, accessibility: Dict, indexing: Dict,
                                    analysis_data: Dict,
                                    acc_score: int, idx_score: int, cnt_score: int) -> str:
        """상세 분석 텍스트 생성"""
        lines = []
        
        # 접근성 분석
        status_code = accessibility.get('status_code')
        if status_code == 200:
            lines.append(f"✅ 접근성: 직접 접근 가능 (HTTP {status_code})")
        elif 300 <= status_code < 400:
            redirect = accessibility.get('redirect_domain')
            if redirect:
                lines.append(f"⚠️ 접근성: 리다이렉트됨 ({status_code} → {redirect})")
            else:
                lines.append(f"⚠️ 접근성: 리다이렉트 응답 ({status_code})")
        elif status_code and 400 <= status_code < 600:
            lines.append(f"❌ 접근성: {status_code} 에러로 접근 불가")
        else:
            lines.append("❌ 접근성: 접근 불가능")
        
        # 색인 분석
        ahmia = indexing.get('ahmia_found', False)
        ddgo = indexing.get('duckduckgo_found', False)
        
        if ahmia or ddgo:
            engines = []
            if ahmia:
                engines.append(f"Ahmia ({indexing.get('ahmia_results', 0)}건)")
            if ddgo:
                engines.append(f"DuckDuckGo ({indexing.get('duckduckgo_found', False)})")
            lines.append(f"✅ 색인: {', '.join(engines)}에서 검색됨")
        else:
            lines.append("❌ 색인: 검색 엔진에서 미발견 (은닉됨)")
        
        # 콘텐츠 분석
        if analysis_data.get('html_collected'):
            if analysis_data.get('is_illegal'):
                conf = analysis_data.get('illegal_confidence', 0)
                primary_type = analysis_data.get('primary_illegal_type', 'unknown')
                lines.append(f"❌ 콘텐츠: 불법 키워드 탐지 ({primary_type}, 신뢰도 {conf*100:.0f}%)")
            else:
                lines.append("✅ 콘텐츠: 불법 키워드 미탐지")
        else:
            lines.append("❓ 콘텐츠: HTML 수집 실패로 분석 불가")
        
        return "\n".join(lines)
