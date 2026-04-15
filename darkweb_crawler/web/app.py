"""
Darkweb Crawler 웹 인터페이스
로컬 웹 서버에서 도메인 분석 및 보고서 생성
"""

from flask import Flask, render_template, request, jsonify, send_file
import sys
import os
from pathlib import Path
import json
import logging
from datetime import datetime

# 부모 디렉토리 경로 추가
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from agent import DarkwebDomainAgent

# 로깅 설정
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

app = Flask(__name__, 
            template_folder='templates',
            static_folder='static')

app.config['JSON_AS_ASCII'] = False

# 분석 상태 저장 (간단한 메모리 캐시)
analysis_status = {}

# agent 초기화
try:
    agent = DarkwebDomainAgent()
    logger.info("✅ DarkwebDomainAgent 초기화 성공")
except Exception as e:
    logger.error(f"❌ Agent 초기화 실패: {str(e)}")
    agent = None

@app.route('/')
def index():
    """메인 페이지 - 도메인 입력 폼"""
    return render_template('index.html')

@app.route('/api/analyze', methods=['POST'])
def analyze_domain():
    """
    도메인 분석 API
    
    요청:
    {
        "domain": "example.onion"
    }
    
    응답:
    {
        "status": "success/error",
        "message": "분석 중... 또는 오류 메시지",
        "report_path": "생성된 HTML 보고서 경로",
        "domain": "분석한 도메인"
    }
    """
    try:
        data = request.get_json()
        domain = data.get('domain', '').strip()
        
        if not domain:
            return jsonify({
                'status': 'error',
                'message': '도메인을 입력해주세요'
            }), 400
        
        # 도메인 유효성 검사
        if not domain.endswith('.onion'):
            return jsonify({
                'status': 'error',
                'message': '.onion 도메인만 지원합니다'
            }), 400
        
        if not agent:
            return jsonify({
                'status': 'error',
                'message': 'Agent 초기화 실패'
            }), 500
        
        logger.info(f"🔍 분석 시작: {domain}")
        
        # 분석 실행
        result = agent.analyze_domain(domain, verbose=False)
        
        if result['status'] == 'success':
            logger.info(f"✅ 분석 완료: {domain}")
            
            return jsonify({
                'status': 'success',
                'message': f'{domain} 분석이 완료되었습니다',
                'domain': domain,
                'report_path': result.get('report_path', ''),
                'timestamp': datetime.now().isoformat()
            }), 200
        else:
            logger.error(f"❌ 분석 실패: {domain} - {result.get('error')}")
            
            return jsonify({
                'status': 'error',
                'message': f'분석 중 오류 발생: {result.get("error")}',
                'domain': domain
            }), 500
    
    except Exception as e:
        logger.error(f"❌ API 오류: {str(e)}", exc_info=True)
        return jsonify({
            'status': 'error',
            'message': f'서버 오류: {str(e)}'
        }), 500

@app.route('/api/report/<domain>')
def get_report(domain):
    """
    생성된 보고서 조회
    
    Args:
        domain: .onion 도메인
    
    Returns:
        HTML 보고서 또는 에러 메시지
    """
    try:
        # 가장 최신 보고서 파일 찾기
        report_dir = Path(__file__).parent.parent / 'analysis_reports'
        
        if not report_dir.exists():
            return jsonify({
                'status': 'error',
                'message': '보고서 디렉토리를 찾을 수 없습니다'
            }), 404
        
        # 도메인 관련 파일 검색 (최신 파일)
        domain_clean = domain.replace('.', '_')
        report_files = sorted(
            report_dir.glob(f'*{domain_clean}*'),
            key=os.path.getmtime,
            reverse=True
        )
        
        if not report_files:
            return jsonify({
                'status': 'error',
                'message': f'{domain}의 보고서를 찾을 수 없습니다'
            }), 404
        
        report_path = report_files[0]
        
        logger.info(f"📊 보고서 반환: {report_path}")
        
        # HTML 파일 반환
        with open(report_path, 'r', encoding='utf-8') as f:
            html_content = f.read()
        
        return html_content, 200, {'Content-Type': 'text/html; charset=utf-8'}
    
    except Exception as e:
        logger.error(f"❌ 보고서 조회 오류: {str(e)}")
        return jsonify({
            'status': 'error',
            'message': f'보고서 조회 중 오류: {str(e)}'
        }), 500

@app.route('/results')
def results():
    """분석 결과 페이지"""
    domain = request.args.get('domain', '')
    
    if not domain:
        return """
        <html>
            <head><title>오류</title></head>
            <body>
                <h1>❌ 오류</h1>
                <p>도메인이 지정되지 않았습니다</p>
                <a href="/">← 돌아가기</a>
            </body>
        </html>
        """, 400
    
    return render_template('result.html', domain=domain)

@app.route('/health')
def health():
    """헬스 체크"""
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.now().isoformat()
    }), 200

@app.errorhandler(404)
def not_found(error):
    """404 에러 핸들러"""
    return jsonify({
        'status': 'error',
        'message': '페이지를 찾을 수 없습니다'
    }), 404

@app.errorhandler(500)
def server_error(error):
    """500 에러 핸들러"""
    return jsonify({
        'status': 'error',
        'message': '서버 오류가 발생했습니다'
    }), 500

if __name__ == '__main__':
    logger.info("🌐 Darkweb Crawler 웹 서버 시작")
    logger.info("📍 http://localhost:5000 에서 접속 가능합니다")
    
    # 개발 모드에서 실행
    app.run(
        host='127.0.0.1',
        port=5000,
        debug=True,
        use_reloader=False  # Agent 초기화 중복 방지
    )
