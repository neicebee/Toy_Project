"""
HTML 정제 유틸리티 - 중복 코드 통합
모든 분석 모듈에서 사용하는 HTML 정제 로직 중앙화
"""

import re
import logging

logger = logging.getLogger(__name__)


class HTMLCleaner:
    """HTML 콘텐츠 정제 (script, style, 태그 제거)"""
    
    # 정제 패턴 (클래스 변수로 미리 컴파일)
    _patterns = [
        re.compile(r'<script[^>]*>.*?</script>', re.IGNORECASE | re.DOTALL),
        re.compile(r'<style[^>]*>.*?</style>', re.IGNORECASE | re.DOTALL),
        re.compile(r'<[^>]+>'),
    ]
    
    @staticmethod
    def clean(html: str) -> str:
        """
        HTML 콘텐츠 정제
        
        프로세스:
        1. <script> 태그 제거
        2. <style> 태그 제거
        3. 모든 HTML 태그 제거
        4. 연속 공백 정리
        
        Args:
            html: 정제할 HTML 문자열
        
        Returns:
            정제된 텍스트 문자열
        
        Example:
            >>> html = '<p>Hello <b>world</b></p><script>alert("x")</script>'
            >>> HTMLCleaner.clean(html)
            'Hello world'
        """
        if not html:
            return ""
        
        text = html
        
        # 각 패턴 적용
        for pattern in HTMLCleaner._patterns:
            text = pattern.sub(' ', text)
        
        # 연속 공백 정리
        text = re.sub(r'\s+', ' ', text)
        
        return text.strip()
