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

from analyzers import ContentAnalyzer, CategoryClassifier
from analyzers.llm_analyzer import LLMAnalyzer
from analyzers.coda_classifier import CoDAClassifier
from reporters.agent_report_generator import AgentReportGenerator
from utils.logger import get_logger

logger = get_logger(__name__)


class DarkwebDomainAgent:
    """어니언 도메인 분석 에이전트"""
    
    def __init__(self, server_url: str = None):
        """
        Args:
            server_url: 우분투 서버 URL
        """
        self.server_url = server_url or os.environ.get("SERVER_URL", "http://127.0.0.1:5001")
        self.analyzer = ContentAnalyzer()
        self.classifier = CategoryClassifier()
        self.coda_classifier = CoDAClassifier()
        self.llm_analyzer = LLMAnalyzer()
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
            
            # 2단계: CoDA 범죄 카테고리 분류 (학습된 분류기)
            logger.info("[2/6] 🎯 CoDA 범죄 카테고리 분류 중...")
            html_content = server_analysis.get('analysis', {}).get('html_content', '')

            if not html_collected:
                logger.warning("   ⚠️ HTML 미수집 - CoDA 분류 스킵")
                coda_result = {
                    'category': 'unknown',
                    'confidence': 0.0,
                    'uncertain': True,
                    'all_probs': {},
                    'available': False,
                    'skip_reason': 'HTML not collected'
                }
            else:
                coda_result = self.coda_classifier.classify(html_content)
                if coda_result['available']:
                    status = "⚠️ 불확실" if coda_result['uncertain'] else "✅"
                    logger.info(
                        f"   {status} CoDA 분류: {coda_result['category']} "
                        f"({coda_result['confidence']:.1%})"
                    )
                else:
                    logger.warning("   ⚠️ CoDA 분류기 미학습 - train_coda_classifier.py 실행 필요")
            logger.info("    ✅ CoDA 분류 완료")

            # content_analysis는 신뢰도 계산용으로만 최소한 유지
            content_analysis = {'is_illegal': False, 'illegal_confidence': 0,
                                'primary_illegal_type': None}

            # 3단계: 사이트 유형 분류 (BART zero-shot)
            logger.info("[3/6] 🏷️ 사이트 유형 분류 중...")
            if html_collected:
                category_result = self.classifier.classify_content(html_content)
                logger.info(f"    ✅ 사이트 유형: {category_result.get('primary_category', 'unknown')}")
            else:
                logger.warning("    ⚠️ HTML 미수집 - 사이트 유형 분류 스킵")
                category_result = {'primary_category': 'unknown', 'secondary_category': None,
                                   'confidence': 0, 'skip_reason': 'HTML not collected'}

            # 4단계: LLM 사이트 요약 분석
            logger.info("[4/6] 🤖 LLM 사이트 요약 분석 중...")
            if html_collected:
                llm_result = self.llm_analyzer.analyze(html_content, domain=domain)
                if llm_result['success']:
                    logger.info(f"    ✅ LLM 분석 완료 | 위험도: {llm_result['risk_level']}")
                else:
                    logger.warning(f"    ⚠️ LLM 분석 실패: {llm_result['error']}")
            else:
                logger.warning("    ⚠️ HTML 미수집 - LLM 분석 스킵")
                llm_result = {'success': False, 'error': 'HTML 미수집'}

            # 4단계: 보고서 생성
            logger.info("[4/5] 📋 보고서 생성 중...")

            report_path = self.report_generator.generate_report(
                domain=domain,
                analysis_result=server_analysis['analysis'],
                coda_result=coda_result,
                llm_result=llm_result,
                category_result=category_result
            )
            
            logger.info("    ✅ 보고서 생성 완료")
            logger.info(f"    📁 보고서 위치: {report_path}\n")
            
            # 결과 구성
            result['status'] = 'success'
            result['analysis'] = {
                'server_data': server_analysis['analysis'],
                'coda_result': coda_result,
                'llm_result': llm_result,
                'report_path': report_path
            }

            # 종합 요약 출력
            self._print_summary(domain, coda_result, server_analysis['analysis'])
            
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
                timeout=360  # 최대 6분
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
    
    def _print_summary(self, domain: str, coda_result: Dict, server_analysis: Dict):
        """분석 결과 요약 출력"""
        logger.info("="*60)
        logger.info("📊 분석 결과 요약")
        logger.info("="*60)
        logger.info(f"\n도메인: {domain}")

        indexing = server_analysis.get('indexing', {})
        logger.info(f"\n📋 검색 색인:")
        logger.info(f"  • Ahmia: {'✅' if indexing.get('ahmia_found') else '❌'} ({indexing.get('ahmia_results', 0)}건)")
        logger.info(f"  • DuckDuckGo: {'✅' if indexing.get('duckduckgo_found') else '❌'}")

        logger.info(f"\n🎯 CoDA 범죄 카테고리:")
        if coda_result.get('available'):
            status = "⚠️ 불확실" if coda_result['uncertain'] else "✅"
            logger.info(f"  • {status} {coda_result['category'].upper()} ({coda_result['confidence']:.1%})")
            top3 = list(coda_result.get('all_probs', {}).items())[:3]
            if top3:
                logger.info("  • 상위 3개: " + " | ".join(f"{k}={v:.1%}" for k, v in top3))
        else:
            logger.info("  • 학습된 모델 없음 (train_coda_classifier.py 실행 필요)")

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
        default='http://127.0.0.1:5001',
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
