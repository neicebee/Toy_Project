"""
로거 유틸리티 - 감시 추적 및 감사 기록
"""

import logging
import os
from datetime import datetime
from pathlib import Path

# config.config에서 임포트 시도, 실패하면 기본값 사용
try:
    from config.config import LOG_DIR, AUDIT_LOG_FILE, LOG_LEVEL, LOG_FORMAT, AUDIT_TRAIL_ENABLED
except ImportError:
    # 우분투 서버 환경: 기본값 사용
    current_dir = Path(__file__).parent.parent
    LOG_DIR = str(current_dir / 'logs')
    AUDIT_LOG_FILE = str(current_dir / 'logs' / 'audit.log')
    LOG_LEVEL = 'INFO'
    LOG_FORMAT = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    AUDIT_TRAIL_ENABLED = True

os.makedirs(LOG_DIR, exist_ok=True)

# 기본 로거 설정
def get_logger(name: str, log_file: str = None) -> logging.Logger:
    """로거 인스턴스 반환"""
    logger = logging.getLogger(name)
    logger.setLevel(getattr(logging, LOG_LEVEL))
    
    if not logger.handlers:
        # 파일 핸들러
        if log_file is None:
            log_file = os.path.join(LOG_DIR, f'{name.replace(".", "_")}.log')
        
        fh = logging.FileHandler(log_file, encoding='utf-8')
        fh.setLevel(getattr(logging, LOG_LEVEL))
        
        # 포매터
        formatter = logging.Formatter(LOG_FORMAT)
        fh.setFormatter(formatter)
        
        logger.addHandler(fh)
        
        # 콘솔 핸들러
        ch = logging.StreamHandler()
        ch.setLevel(getattr(logging, LOG_LEVEL))
        ch.setFormatter(formatter)
        logger.addHandler(ch)
    
    return logger

# 감사 추적 로거 (정부 기관 요구사항)
class AuditTrailLogger:
    """감시 추적용 감사 기록 로거"""
    
    def __init__(self):
        self.logger = logging.getLogger('audit_trail')
        self.logger.setLevel(logging.INFO)
        
        if not self.logger.handlers and AUDIT_TRAIL_ENABLED:
            os.makedirs(os.path.dirname(AUDIT_LOG_FILE), exist_ok=True)
            
            handler = logging.FileHandler(AUDIT_LOG_FILE, encoding='utf-8')
            handler.setLevel(logging.INFO)
            
            formatter = logging.Formatter('%(asctime)s | %(message)s')
            handler.setFormatter(formatter)
            
            self.logger.addHandler(handler)
    
    def log_scan_start(self, scan_id: str):
        """스캔 시작 기록"""
        msg = f"SCAN_START | ID={scan_id} | User=SYSTEM"
        if AUDIT_TRAIL_ENABLED:
            self.logger.info(msg)
    
    def log_domain_check(self, domain: str, accessible: bool, indexed: bool, concealed: bool):
        """도메인 검사 결과 기록"""
        msg = f"DOMAIN_CHECK | Domain={domain} | Accessible={accessible} | Indexed={indexed} | Concealed={concealed}"
        if AUDIT_TRAIL_ENABLED:
            self.logger.info(msg)
    
    def log_report_generated(self, report_path: str, record_count: int):
        """리포트 생성 기록"""
        msg = f"REPORT_GENERATED | Path={report_path} | Records={record_count}"
        if AUDIT_TRAIL_ENABLED:
            self.logger.info(msg)
    
    def log_domain_update(self, old_domain: str, new_domain: str, reason: str):
        """도메인 업데이트 기록"""
        msg = f"DOMAIN_UPDATE | OldDomain={old_domain} | NewDomain={new_domain} | Reason={reason}"
        if AUDIT_TRAIL_ENABLED:
            self.logger.info(msg)
    
    def log_error(self, error_msg: str, error_type: str = "ERROR"):
        """오류 기록"""
        msg = f"{error_type} | {error_msg}"
        if AUDIT_TRAIL_ENABLED:
            self.logger.info(msg)
    
    def log_event(self, event_type: str, description: str):
        """일반 이벤트 기록"""
        msg = f"{event_type} | {description}"
        if AUDIT_TRAIL_ENABLED:
            self.logger.info(msg)

# 전역 감사 추적 로거
audit_logger = AuditTrailLogger()
