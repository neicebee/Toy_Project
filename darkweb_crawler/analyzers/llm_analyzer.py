"""
LLM 기반 다크웹 사이트 분석기 - OpenAI API 사용
HTML 콘텐츠를 분석하여 사이트 목적/특성 자연어 요약
"""

import os
import logging
import re
import json
from typing import Dict, Optional

from utils.html_cleaner import HTMLCleaner

logger = logging.getLogger(__name__)

SYSTEM_PROMPT = """당신은 다크웹 사이트 분석 전문가입니다. 제공된 웹페이지 텍스트를 분석하여 다음 항목을 JSON 형식으로 반환하세요.

반드시 아래 JSON 형식만 출력하세요 (다른 텍스트 없이):
{
  "summary": "사이트에 대한 2~3문장 요약 (한국어)",
  "purpose": "사이트의 주요 목적 (한국어, 1문장)",
  "site_type": "사이트 유형 (예: 마켓플레이스, 포럼, 서비스, 정보제공 등)",
  "risk_level": "위험도 (낮음/중간/높음/매우높음)",
  "risk_reason": "위험도 판단 이유 (한국어, 1문장)",
  "notable_features": ["주목할 특징 1", "주목할 특징 2"],
  "language": "사이트 주요 언어"
}

분석 기준:
- 콘텐츠 내용에 기반하여 객관적으로 분석
- 마약, 무기, 불법 콘텐츠 등은 위험도를 높게 평가
- 포럼, 정보공유, 개인정보보호 서비스는 낮게 평가
- 언어가 불명확하면 "알 수 없음"으로 표기"""


class LLMAnalyzer:
    """OpenAI API를 사용한 사이트 요약 분석기"""

    def __init__(self, api_key: Optional[str] = None,
                 model: str = "gpt-4o-mini",
                 max_input_chars: int = 8000):
        self.api_key = api_key or os.environ.get("OPENAI_API_KEY")
        self.model = model
        self.max_input_chars = max_input_chars
        self._client = None

        if not self.api_key:
            logger.warning("⚠️ OPENAI_API_KEY 미설정 - LLM 분석 비활성화")

    def _get_client(self):
        if self._client is not None:
            return self._client

        try:
            from openai import OpenAI
            self._client = OpenAI(api_key=self.api_key)
            return self._client
        except ImportError:
            logger.error("❌ openai 패키지 미설치: pip install openai")
            return None

    def analyze(self, html: str, domain: str = "") -> Dict:
        empty_result = {
            'success': False,
            'summary': None,
            'purpose': None,
            'site_type': None,
            'risk_level': None,
            'risk_reason': None,
            'notable_features': [],
            'language': None,
            'model_used': self.model,
            'error': None
        }

        if not self.api_key:
            empty_result['error'] = 'API 키 미설정'
            return empty_result

        if not html or not html.strip():
            empty_result['error'] = 'HTML 없음'
            return empty_result

        client = self._get_client()
        if client is None:
            empty_result['error'] = 'openai 패키지 미설치'
            return empty_result

        text = HTMLCleaner.clean(html)
        if not text:
            empty_result['error'] = '분석할 텍스트 없음'
            return empty_result

        if len(text) > self.max_input_chars:
            text = text[:self.max_input_chars] + "... (이하 생략)"

        domain_hint = f"도메인: {domain}\n\n" if domain else ""
        user_content = f"{domain_hint}웹페이지 텍스트:\n{text}"

        try:
            logger.info(f"🤖 LLM 분석 시작: {domain} (모델: {self.model})")

            response = client.chat.completions.create(
                model=self.model,
                max_tokens=512,
                messages=[
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": user_content}
                ]
            )

            raw = response.choices[0].message.content.strip()

            json_match = re.search(r'\{.*\}', raw, re.DOTALL)
            if not json_match:
                raise ValueError(f"JSON 응답 파싱 실패: {raw[:100]}")

            parsed = json.loads(json_match.group())

            logger.info(f"✅ LLM 분석 완료: {domain} | 위험도={parsed.get('risk_level')}")

            return {
                'success': True,
                'summary': parsed.get('summary', ''),
                'purpose': parsed.get('purpose', ''),
                'site_type': parsed.get('site_type', ''),
                'risk_level': parsed.get('risk_level', ''),
                'risk_reason': parsed.get('risk_reason', ''),
                'notable_features': parsed.get('notable_features', []),
                'language': parsed.get('language', ''),
                'model_used': self.model,
                'error': None
            }

        except Exception as e:
            logger.error(f"❌ LLM 분석 오류: {e}")
            empty_result['error'] = str(e)
            return empty_result
