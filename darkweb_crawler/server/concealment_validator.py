"""
악성/차단 도메인 확인 (악성/차단 도메인 DB)

주의: 이 모듈은 악성 또는 차단된 도메인을 확인하며,
실제 "은닉" 판정은 app.py에서 다음 기준으로 수행됩니다:
- 악성 도메인 OR
- 차단된 도메인 OR  
- Ahmia에서 색인되지 않은 도메인
→ 위 중 하나라도 해당하면 "은닉됨" (is_concealed = True)
"""

import os
import logging
from typing import Dict

logger = logging.getLogger(__name__)

class ConcealmentValidator:
    """악성/차단 도메인 확인 (은닉 판정의 한 부분)"""
    
    def __init__(self):
        # 파일 경로 (실제 파일은 미존재 - 초기화만 수행)
        self.malicious_domains = set()
        self.blocked_domains = set()
        logger.info("ConcealmentValidator 초기화됨")
    
    def check_concealment(self, domain: str) -> Dict:
        """
        도메인이 악성 또는 차단되었는지 확인
        
        반환값:
            is_concealed: 악성이거나 차단된 도메인이면 True
            (최종 "은닉" 판정은 app.py에서 색인 여부와 조합)
        """
        domain_clean = domain.lower().replace('http://', '').replace('https://', '')
        
        is_malicious = domain_clean in self.malicious_domains
        is_blocked = domain_clean in self.blocked_domains
        is_concealed = is_malicious or is_blocked
        
        if is_malicious and is_blocked:
            reason = "악성 + 차단됨"
        elif is_malicious:
            reason = "악성 도메인"
        elif is_blocked:
            reason = "차단된 도메인"
        else:
            reason = "정상 (악성/차단 아님)"
        
        logger.info(f"악성/차단 확인: {domain_clean} - 악성/차단: {is_concealed} ({reason})")
        
        return {
            'domain': domain_clean,
            'is_concealed': is_concealed,
            'reason': reason,
            'is_malicious': is_malicious,
            'is_blocked': is_blocked
        }
    
    def batch_check(self, domains: list) -> list:
        """여러 도메인 일괄 확인"""
        results = []
        
        for domain in domains:
            result = self.check_concealment(domain)
            results.append(result)
        
        return results
