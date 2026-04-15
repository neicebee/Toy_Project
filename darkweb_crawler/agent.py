#!/usr/bin/env python3
"""
Darkweb Domain Analysis Agent - 메인 에이전트
단일 어니언 도메인을 입력받아 종합 분석 수행
"""

import sys
import os
import logging
import argparse
import json
import requests
from pathlib import Path
from datetime import datetime
from typing import Dict

# 부모 디렉토리를 경로에 추가
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from analyzers import ContentAnalyzer, CategoryClassifier, TrustScorer
from reporters.agent_report_generator import AgentReportGenerator
from utils.logger import get_logger

logger = get_logger(__name__)


class DarkwebDomainAgent:
    """어니언 도메인 분석 에이전트"""
    
    def __init__(self, server_url: str = "http://192.168.64.7:5000"):
        """
        Args:
            server_url: 우분투 서버 URL
        """
        self.server_url = server_url
        self.analyzer = ContentAnalyzer()
        self.classifier = CategoryClassifier()
        self.scorer = TrustScorer()
        self.report_generator = AgentReportGenerator()
        
        logger.info(f"🚀 에이전트 초기화: 서버={server_url}")
    
    def analyze_domain(self, domain: str, verbose: bool = False) -> Dict:
        """
        도메인 종합 분석
        
        Args:
            domain: 분석할 .onion 도메인
            verbose: 상세 정보 출력
        
        Returns:
            분석 결과 딕셔너리
        """
        logger.info(f"\n{'='*60}")
        logger.info(f"🔍 도메인 분석 시작: {domain}")
        logger.info(f"{'='*60}\n")
        
        result = {
            'domain': domain,
            'timestamp': datetime.now().isoformat(),
            'status': 'pending',
            'analysis': None,
            'error': None
        }
        
        try:
            # 1단계: 서버에서 데이터 수집
            logger.info("[1/4] 🌐 서버에서 데이터 수집 중...")
            server_analysis = self._fetch_server_analysis(domain)
            
            if not server_analysis:
                result['status'] = 'error'
                result['error'] = '서버 분석 실패'
                return result
            
            logger.info("    ✅ 데이터 수집 완료")
            
            # HTML 수집 상태 확인
            html_collected = server_analysis['analysis'].get('html_collected', False)
            if not html_collected:
                logger.warning("⚠️ 도메인 접근 불가 - HTML을 수집하지 못했습니다")
                logger.warning(f"   경고: {server_analysis['analysis'].get('analysis_warning', '원인 불명')}")
            
            # 2단계: 콘텐츠 분석
            logger.info("[2/4] 📄 콘텐츠 분석 중...")
            html_content = server_analysis.get('analysis', {}).get('html_content', '')
            
            # HTML이 수집되지 못한 경우 분석 스킵
            if not html_collected:
                logger.warning("   ⚠️ HTML 미수집 - 콘텐츠 분석 스킵")
                content_analysis = {
                    'is_illegal': False,
                    'illegal_confidence': 0,
                    'illegal_types': [],
                    'primary_illegal_type': None,
                    'skip_reason': 'HTML not collected'
                }
            else:
                content_analysis = self.analyzer.detect_illegal_content(
                    html_content,
                    verbose=verbose
                )
            logger.info("    ✅ 콘텐츠 분석 완료")
            
            # 3단계: 카테고리 분류
            logger.info("[3/4] 🏷️ 카테고리 분류 중...")
            
            # HTML이 수집되지 못한 경우 분류 스킵
            if not html_collected:
                logger.warning("   ⚠️ HTML 미수집 - 카테고리 분류 스킵")
                category_result = {
                    'primary_category': 'unknown',
                    'primary_confidence': 0,
                    'confidence': 0,
                    'skip_reason': 'HTML not collected'
                }
            else:
                category_result = self.classifier.classify_content(
                    html_content,
                    verbose=verbose
                )
            logger.info("    ✅ 카테고리 분류 완료")
            
            # 4단계: 신뢰도 계산
            logger.info("[4/4] 📊 신뢰도 점수 계산 중...")
            
            # 로컬 분석 데이터와 서버 데이터 결합
            trust_input = {
                **server_analysis['analysis'],
                'is_illegal': content_analysis.get('is_illegal', False),
                'illegal_confidence': content_analysis.get('illegal_confidence', 0),
                'primary_illegal_type': content_analysis.get('primary_illegal_type')
            }
            
            trust_analysis = self.scorer.calculate_comprehensive_trust(trust_input)
            logger.info("    ✅ 신뢰도 계산 완료")
            
            # 5단계: 보고서 생성
            logger.info("[5/5] 📋 보고서 생성 중...")
            
            report_path = self.report_generator.generate_report(
                domain=domain,
                analysis_result=server_analysis['analysis'],
                trust_analysis=trust_analysis,
                content_analysis=content_analysis,
                category_result=category_result
            )
            
            logger.info("    ✅ 보고서 생성 완료")
            logger.info(f"    📁 보고서 위치: {report_path}\n")
            
            # 결과 구성
            result['status'] = 'success'
            result['analysis'] = {
                'server_data': server_analysis['analysis'],
                'content_analysis': content_analysis,
                'category_result': category_result,
                'trust_analysis': trust_analysis,
                'report_path': report_path
            }
            
            # 종합 요약 출력
            self._print_summary(domain, trust_analysis, content_analysis, category_result, server_analysis['analysis'])
            
        except Exception as e:
            logger.error(f"❌ 분석 중 오류: {str(e)}", exc_info=True)
            result['status'] = 'error'
            result['error'] = str(e)
        
        return result
    
    def _fetch_server_analysis(self, domain: str) -> Dict:
        """
        우분투 서버에서 분석 데이터 수집
        
        Returns:
            {'analysis': {...}, 'error': None} 또는 None
        """
        try:
            endpoint = f"{self.server_url}/api/analyze_domain"
            payload = {'domain': domain}
            
            logger.debug(f"   요청: POST {endpoint}")
            logger.debug(f"   페이로드: {json.dumps(payload)}")
            
            response = requests.post(
                endpoint,
                json=payload,
                timeout=120  # 최대 2분
            )
            
            if response.status_code != 200:
                logger.error(f"❌ 서버 응답 오류: {response.status_code}")
                logger.error(f"   본문: {response.text}")
                return None
            
            result = response.json()
            
            if not result.get('success'):
                logger.error(f"❌ 서버 분석 실패: {result.get('error')}")
                return None
            
            logger.debug(f"   응답: {json.dumps(result, indent=2)[:200]}...")
            
            return result
        
        except requests.exceptions.Timeout:
            logger.error("❌ 서버 연결 타임아웃 (서버가 응답하지 않음)")
            return None
        
        except requests.exceptions.ConnectionError:
            logger.error(f"❌ 서버 연결 실패 ({self.server_url}에 연결할 수 없음)")
            return None
        
        except Exception as e:
            logger.error(f"❌ 서버 분석 중 오류: {str(e)}")
            return None
    
    def _print_summary(self, domain: str, trust_analysis: Dict,
                      content_analysis: Dict, category_result: Dict,
                      server_analysis: Dict):
        """분석 결과 요약 출력"""
        
        logger.info("="*60)
        logger.info("📊 분석 결과 요약")
        logger.info("="*60)
        
        # 신뢰도
        trust_level = trust_analysis['trust_level']
        trust_level_ko = {
            'HIGHLY_TRUSTWORTHY': '매우 높음 ✅',
            'TRUSTWORTHY': '높음 ✅',
            'SUSPICIOUS': '중간 (의심) ⚠️',
            'UNTRUSTED': '낮음 (위험) ❌',
            'UNREACHABLE': '분석 불가 ❓'
        }.get(trust_level, '알 수 없음')
        
        logger.info(f"\n도메인: {domain}")
        logger.info(f"신뢰도: {trust_analysis['total_score']}/100 ({trust_level_ko})")
        
        # 점수 분석
        logger.info(f"\n점수 분석:")
        logger.info(f"  • 접근성:     {trust_analysis['score_breakdown']['accessibility']['score']:3d}/40점")
        logger.info(f"  • 색인 신뢰도: {trust_analysis['score_breakdown']['indexing']['score']:3d}/30점")
        logger.info(f"  • 콘텐츠:     {trust_analysis['score_breakdown']['content']['score']:3d}/30점")
        
        # 색인 정보
        indexing = server_analysis.get('indexing', {})
        ahmia_status = '✅' if indexing.get('ahmia_found') else '❌'
        ddgo_status = '✅' if indexing.get('duckduckgo_found') else '❌'
        
        logger.info(f"\n📋 검색 색인 정보:")
        logger.info(f"  • Ahmia: {ahmia_status} (결과: {indexing.get('ahmia_results', 0)}건)")
        logger.info(f"  • DuckDuckGo: {ddgo_status}")
        
        # 불법성
        if content_analysis['is_illegal']:
            logger.warning(f"\n⚠️ 불법 콘텐츠 탐지:")
            logger.warning(f"  • 주요 유형: {content_analysis['primary_illegal_type']}")
            logger.warning(f"  • 신뢰도: {content_analysis['illegal_confidence']*100:.1f}%")
        else:
            logger.info(f"\n✅ 불법 콘텐츠 미탐지")
        
        # 카테고리
        logger.info(f"\n사이트 카테고리:")
        logger.info(f"  • 주요: {category_result.get('primary_category', 'unknown').replace('_', ' ').title()}")
        if category_result.get('secondary_category'):
            logger.info(f"  • 부가: {category_result['secondary_category'].replace('_', ' ').title()}")
        logger.info(f"  • 신뢰도: {category_result.get('confidence', 0)*100:.1f}%")
        
        logger.info(f"\n{'='*60}\n")


def main():
    """메인 함수"""
    parser = argparse.ArgumentParser(
        description='어니언 도메인 분석 에이전트',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
예시:
  python agent.py mdgdxj5d7wlgq3m6e4fvca5sgnbvejsyjh3sqh3p3tfngcnrhpuy3aid.onion
  python agent.py -s http://localhost:5000 example.onion -v
"""
    )
    
    parser.add_argument(
        'domain',
        help='분석할 .onion 도메인'
    )
    
    parser.add_argument(
        '-s', '--server',
        default='http://192.168.64.7:5000',
        help='우분투 서버 주소 (기본값: http://192.168.64.7:5000)'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='상세 정보 출력'
    )
    
    args = parser.parse_args()
    
    # 도메인 검증
    if not args.domain.endswith('.onion'):
        print("❌ 오류: 올바른 .onion 도메인을 입력하세요")
        sys.exit(1)
    
    # 에이전트 생성 및 실행
    agent = DarkwebDomainAgent(server_url=args.server)
    result = agent.analyze_domain(args.domain, verbose=args.verbose)
    
    # 결과 출력
    if result['status'] == 'success':
        logger.info("✅ 분석 완료")
        
        # 보고서 경로 출력
        report_path = result['analysis'].get('report_path')
        if report_path:
            logger.info(f"\n📄 보고서 생성 완료:")
            logger.info(f"   {report_path}")
        
        sys.exit(0)
    else:
        logger.error(f"❌ 분석 실패: {result['error']}")
        sys.exit(1)


if __name__ == '__main__':
    main()
