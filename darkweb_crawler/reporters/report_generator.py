"""리포트 생성기 - JSON/CSV/HTML 형식의 정부 제출용 리포트"""

import json
import csv
import io
import base64
from datetime import datetime
from pathlib import Path
from config.config import REPORT_DIR, REPORT_FORMATS, REPORT_TIMESTAMP_FORMAT
from utils.logger import get_logger
from database.db_manager import DatabaseManager

try:
    import matplotlib
    matplotlib.use('Agg')  # GUI 없이 렌더링
    import matplotlib.pyplot as plt
    # macOS 한글 폰트 설정
    plt.rcParams['font.family'] = 'AppleGothic'
    plt.rcParams['axes.unicode_minus'] = False
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    plt = None

logger = get_logger(__name__)

class ReportGenerator:
    """도메인 스캔 결과 리포트 생성"""
    
    def __init__(self):
        self.report_dir = Path(REPORT_DIR)
        self.report_dir.mkdir(parents=True, exist_ok=True)
        self.db = DatabaseManager()
    
    def generate_report(self, scan_id: str, format_type: str = 'json') -> str:
        """
        리포트 생성
        
        Args:
            scan_id: 스캔 ID
            format_type: 'json', 'csv', 또는 'html'
        
        Returns:
            생성된 리포트 파일 경로
        """
        
        # 스캔 결과 조회
        results = self.db.get_scan_results(scan_id)
        
        if not results:
            logger.warning(f"스캔 결과 없음: {scan_id}")
            return None
        
        timestamp = datetime.now().strftime(REPORT_TIMESTAMP_FORMAT)
        
        if format_type == 'json':
            return self._generate_json_report(scan_id, results, timestamp)
        elif format_type == 'csv':
            return self._generate_csv_report(scan_id, results, timestamp)
        elif format_type == 'html':
            return self._generate_html_report(scan_id, results, timestamp)
        else:
            logger.error(f"지원하지 않는 형식: {format_type}")
            return None
    
    def _generate_json_report(self, scan_id: str, results: list, timestamp: str) -> str:
        """JSON 형식 리포트 생성"""
        
        report_data = {
            'scan_id': scan_id,
            'scan_timestamp': timestamp,
            'total_domains': len(results),
            'summary': self._calculate_summary(results),
            'domains': []
        }
        
        for result in results:
            domain = result.get('domain')
            
            # 📌 extra_data에서 도메인 업데이트 정보 추출
            extra_data = {}
            extra_data_str = result.get('extra_data')
            if extra_data_str:
                try:
                    extra_data = json.loads(extra_data_str)
                except:
                    extra_data = {}
            
            domain_record = {
                'domain': domain,
                'accessibility': {
                    'is_accessible': bool(result.get('is_accessible')),
                    'status_code': result.get('status_code'),
                    'response_time': result.get('response_time')
                },
                'indexing': {
                    'is_indexed': bool(result.get('is_indexed')),
                    'search_engine_results': result.get('result_count', 0)
                },
                'concealment': {
                    'is_concealed': bool(result.get('is_concealed')),
                    'is_malicious': bool(result.get('is_malicious')),
                    'is_blocked': bool(result.get('is_blocked')),
                    'reason': result.get('concealment_reason')
                },
                'last_checked': result.get('scan_timestamp')
            }
            
            # 📌 도메인이 업데이트되었으면 정보 추가
            if 'updated_domain' in extra_data:
                domain_record['update_info'] = {
                    'updated': True,
                    'new_domain': extra_data['updated_domain'],
                    'original_domain': extra_data.get('original_domain')
                }
            
            report_data['domains'].append(domain_record)
        
        # 파일 저장
        file_path = self.report_dir / f'report_{scan_id}_{timestamp}.json'
        
        try:
            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(report_data, f, ensure_ascii=False, indent=2)
            
            logger.info(f"JSON 리포트 생성: {file_path}")
            return str(file_path)
        
        except Exception as e:
            logger.error(f"JSON 리포트 생성 실패: {str(e)}")
            return None
    
    def _generate_csv_report(self, scan_id: str, results: list, timestamp: str) -> str:
        """CSV 형식 리포트 생성"""
        
        file_path = self.report_dir / f'report_{scan_id}_{timestamp}.csv'
        
        try:
            rows = []
            
            for result in results:
                row = {
                    '도메인': result.get('domain'),
                    '접근_가능': '예' if result.get('is_accessible') else '아니오',
                    'HTTP_상태코드': result.get('status_code') or 'N/A',
                    '응답시간_초': result.get('response_time') or 'N/A',
                    '검색엔진_색인됨': '예' if result.get('is_indexed') else '아니오',
                    '검색결과_개수': result.get('result_count', 0),
                    '은닉_여부': '예' if result.get('is_concealed') else '아니오',
                    '악성_도메인': '예' if result.get('is_malicious') else '아니오',
                    '차단_목록': '예' if result.get('is_blocked') else '아니오',
                    '은닉_사유': result.get('concealment_reason', '없음'),
                    '검사_일시': result.get('scan_timestamp')
                }
                rows.append(row)
            
            if rows:
                with open(file_path, 'w', newline='', encoding='utf-8-sig') as f:
                    writer = csv.DictWriter(f, fieldnames=rows[0].keys())
                    writer.writeheader()
                    writer.writerows(rows)
                
                logger.info(f"CSV 리포트 생성: {file_path}")
                return str(file_path)
            
            return None
        
        except Exception as e:
            logger.error(f"CSV 리포트 생성 실패: {str(e)}")
            return None
    
    def _calculate_summary(self, results: list) -> dict:
        """스캔 결과 요약 통계 계산"""
        
        accessible_count = sum(1 for r in results if r.get('is_accessible'))
        indexed_count = sum(1 for r in results if r.get('is_indexed'))
        concealed_count = sum(1 for r in results if r.get('is_concealed'))
        malicious_count = sum(1 for r in results if r.get('is_malicious'))
        blocked_count = sum(1 for r in results if r.get('is_blocked'))
        
        return {
            'accessible_domains': accessible_count,
            'indexed_domains': indexed_count,
            'concealed_domains': concealed_count,
            'malicious_domains': malicious_count,
            'blocked_domains': blocked_count,
            'accessibility_rate': f"{(accessible_count / len(results) * 100):.1f}%" if results else "0%",
            'indexing_rate': f"{(indexed_count / len(results) * 100):.1f}%" if results else "0%"
        }
    
    def _generate_charts(self, results: list) -> dict:
        """
        차트 생성 (base64 인코딩된 이미지)
        
        Returns:
            {
                'accessibility': base64 이미지,
                'indexing': base64 이미지,
                'concealment': base64 이미지
            }
        """
        if not MATPLOTLIB_AVAILABLE:
            logger.warning("matplotlib 미설치 - 차트 생성 불가")
            return {}
        
        try:
            charts = {}
            summary = self._calculate_summary(results)
            
            # 차트 1: 접근성
            plt.figure(figsize=(8, 6))
            accessible = summary['accessible_domains']
            not_accessible = len(results) - accessible
            
            colors1 = ['#2ecc71', '#e74c3c']  # 녹색, 빨강
            plt.pie(
                [accessible, not_accessible],
                labels=['접근 가능', '접근 불가'],
                autopct='%1.1f%%',
                colors=colors1,
                startangle=90
            )
            plt.title('접근성 현황', fontsize=14, fontweight='bold', pad=20)
            
            img_buffer = io.BytesIO()
            plt.savefig(img_buffer, format='png', bbox_inches='tight', dpi=100)
            img_buffer.seek(0)
            charts['accessibility'] = base64.b64encode(img_buffer.read()).decode()
            plt.close()
            
            # 차트 2: 색인 여부
            plt.figure(figsize=(8, 6))
            indexed = summary['indexed_domains']
            not_indexed = len(results) - indexed
            
            colors2 = ['#3498db', '#95a5a6']  # 파랑, 회색
            plt.pie(
                [indexed, not_indexed],
                labels=['색인됨', '미색인'],
                autopct='%1.1f%%',
                colors=colors2,
                startangle=90
            )
            plt.title('검색 엔진 색인 현황', fontsize=14, fontweight='bold', pad=20)
            
            img_buffer = io.BytesIO()
            plt.savefig(img_buffer, format='png', bbox_inches='tight', dpi=100)
            img_buffer.seek(0)
            charts['indexing'] = base64.b64encode(img_buffer.read()).decode()
            plt.close()
            
            # 차트 3: 은닉 여부
            plt.figure(figsize=(8, 6))
            concealed = summary['concealed_domains']
            not_concealed = len(results) - concealed
            
            colors3 = ['#e67e22', '#27ae60']  # 주황, 초록
            plt.pie(
                [concealed, not_concealed],
                labels=['은닉됨', '정상'],
                autopct='%1.1f%%',
                colors=colors3,
                startangle=90
            )
            plt.title('은닉 도메인 현황', fontsize=14, fontweight='bold', pad=20)
            
            img_buffer = io.BytesIO()
            plt.savefig(img_buffer, format='png', bbox_inches='tight', dpi=100)
            img_buffer.seek(0)
            charts['concealment'] = base64.b64encode(img_buffer.read()).decode()
            plt.close()
            
            logger.info("✅ 차트 생성 완료 (3개)")
            return charts
        
        except Exception as e:
            logger.error(f"차트 생성 실패: {str(e)}")
            return {}
    
    def _generate_html_report(self, scan_id: str, results: list, timestamp: str) -> str:
        """
        HTML 대시보드 리포트 생성
        
        특징:
        - 반응형 디자인
        - 차트 포함 (pie charts)
        - 색상 코딩 (초록: 정상, 빨강: 문제)
        - 테이블 형식의 도메인 상세 정보
        """
        
        try:
            summary = self._calculate_summary(results)
            charts = self._generate_charts(results)
            
            # 차트 이미지 데이터 (있으면 포함)
            chart_html = ""
            if charts:
                chart_html = f"""
                <div class="charts-container">
                    <div class="chart-item">
                        <img src="data:image/png;base64,{charts.get('accessibility', '')}" alt="접근성 차트">
                    </div>
                    <div class="chart-item">
                        <img src="data:image/png;base64,{charts.get('indexing', '')}" alt="색인 차트">
                    </div>
                    <div class="chart-item">
                        <img src="data:image/png;base64,{charts.get('concealment', '')}" alt="은닉 차트">
                    </div>
                </div>
                """
            
            # 도메인 테이블 생성
            domain_rows = ""
            for result in results:
                status_color = "#2ecc71" if result.get('is_accessible') else "#e74c3c"
                indexed_status = "✓ 색인됨" if result.get('is_indexed') else "✗ 미색인"
                concealed_status = "⚠️ 은닉됨" if result.get('is_concealed') else "✓ 정상"
                
                # 📌 extra_data에서 도메인 업데이트 정보 추출
                domain_display = result.get('domain')
                extra_data_str = result.get('extra_data')
                if extra_data_str:
                    try:
                        extra_data = json.loads(extra_data_str)
                        if 'updated_domain' in extra_data:
                            domain_display = f"{result.get('domain')}<br><small style='color: #e67e22; font-weight: bold;'>→ {extra_data['updated_domain']}</small>"
                    except:
                        pass
                
                domain_rows += f"""
                <tr>
                    <td style="font-family: monospace; word-break: break-all;">{domain_display}</td>
                    <td style="color: {status_color}; font-weight: bold;">
                        {result.get('status_code') or '무응답'}
                    </td>
                    <td>{indexed_status}</td>
                    <td>{result.get('result_count', 0)}건</td>
                    <td style="color: {'#e74c3c' if result.get('is_concealed') else '#2ecc71'}; font-weight: bold;">
                        {concealed_status}
                    </td>
                    <td>{result.get('concealment_reason', '-')}</td>
                </tr>
                """
            
            # HTML 템플릿
            html_content = f"""
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>다크웹 모니터링 - 리포트</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}
        
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }}
        
        .container {{
            max-width: 1400px;
            margin: 0 auto;
            background: white;
            border-radius: 12px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }}
        
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 40px;
            text-align: center;
        }}
        
        .header h1 {{
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }}
        
        .header p {{
            font-size: 1.1em;
            opacity: 0.9;
        }}
        
        .content {{
            padding: 40px;
        }}
        
        .summary-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 40px;
        }}
        
        .summary-card {{
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
            padding: 20px;
            border-radius: 8px;
            text-align: center;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }}
        
        .summary-card h3 {{
            color: #333;
            font-size: 0.9em;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }}
        
        .summary-card .number {{
            font-size: 2.5em;
            font-weight: bold;
            color: #667eea;
        }}
        
        .summary-card .percentage {{
            font-size: 1.2em;
            color: #666;
            margin-top: 5px;
        }}
        
        .charts-container {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 30px;
            margin: 40px 0;
        }}
        
        .chart-item {{
            text-align: center;
            background: #f9f9f9;
            padding: 20px;
            border-radius: 8px;
        }}
        
        .chart-item img {{
            max-width: 100%;
            height: auto;
        }}
        
        .table-section {{
            margin-top: 40px;
        }}
        
        .table-section h2 {{
            color: #333;
            margin-bottom: 20px;
            font-size: 1.5em;
            border-bottom: 3px solid #667eea;
            padding-bottom: 10px;
        }}
        
        table {{
            width: 100%;
            border-collapse: collapse;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        
        thead {{
            background: #667eea;
            color: white;
        }}
        
        th {{
            padding: 15px;
            text-align: left;
            font-weight: 600;
        }}
        
        tbody tr {{
            border-bottom: 1px solid #eee;
            transition: background-color 0.3s;
        }}
        
        tbody tr:hover {{
            background-color: #f9f9f9;
        }}
        
        td {{
            padding: 12px 15px;
            font-size: 0.95em;
        }}
        
        .footer {{
            background: #f0f0f0;
            padding: 20px;
            text-align: center;
            color: #666;
            font-size: 0.9em;
            border-top: 1px solid #ddd;
        }}
        
        .status-accessible {{
            color: #2ecc71;
            font-weight: bold;
        }}
        
        .status-concealed {{
            color: #e74c3c;
            font-weight: bold;
        }}
        
        @media print {{
            body {{
                background: white;
            }}
            .container {{
                box-shadow: none;
                border-radius: 0;
            }}
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📊 다크웹 도메인 모니터링 리포트</h1>
            <p>스캔 ID: {scan_id}</p>
            <p>생성 일시: {timestamp}</p>
        </div>
        
        <div class="content">
            <div class="summary-grid">
                <div class="summary-card">
                    <h3>총 도메인</h3>
                    <div class="number">{len(results)}</div>
                </div>
                <div class="summary-card">
                    <h3>접근 가능</h3>
                    <div class="number">{summary['accessible_domains']}</div>
                    <div class="percentage">{summary['accessibility_rate']}</div>
                </div>
                <div class="summary-card">
                    <h3>색인됨</h3>
                    <div class="number">{summary['indexed_domains']}</div>
                    <div class="percentage">{summary['indexing_rate']}</div>
                </div>
                <div class="summary-card">
                    <h3>은닉됨</h3>
                    <div class="number">{summary['concealed_domains']}</div>
                    <div class="percentage">{(summary['concealed_domains']/len(results)*100):.1f}%</div>
                </div>
            </div>
            
            <h2 style="margin-top: 30px; color: #333;">📈 통계 차트</h2>
            {chart_html}
            
            <div class="table-section">
                <h2>📋 도메인 상세 정보</h2>
                <table>
                    <thead>
                        <tr>
                            <th>도메인</th>
                            <th>HTTP 상태</th>
                            <th>색인 여부</th>
                            <th>검색 결과</th>
                            <th>은닉 여부</th>
                            <th>사유</th>
                        </tr>
                    </thead>
                    <tbody>
                        {domain_rows}
                    </tbody>
                </table>
            </div>
        </div>
        
        <div class="footer">
            <p>정부 감시 기관용 공식 리포트</p>
            <p>© 2026 다크웹 모니터링 시스템 | 기밀 보안 문서</p>
        </div>
    </div>
</body>
</html>
            """
            
            # 파일 저장
            file_path = self.report_dir / f'report_{scan_id}_{timestamp}.html'
            
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(html_content)
            
            logger.info(f"✅ HTML 리포트 생성: {file_path}")
            return str(file_path)
        
        except Exception as e:
            logger.error(f"HTML 리포트 생성 실패: {str(e)}")
            return None
