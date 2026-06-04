"""
에이전트 분석 보고서 생성기 - 단일 도메인 분석 결과 시각화 (HTML)
"""

import re
import base64
import logging
from datetime import datetime
from pathlib import Path
from io import BytesIO
from typing import Dict

try:
    import matplotlib
    from matplotlib import font_manager
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    korean_font_candidates = ['Malgun Gothic', 'AppleGothic', 'NanumGothic', 'Noto Sans CJK KR']
    available_fonts = {font.name for font in font_manager.fontManager.ttflist}
    for font_name in korean_font_candidates:
        if font_name in available_fonts:
            plt.rcParams['font.family'] = font_name
            break
    else:
        plt.rcParams['font.family'] = 'sans-serif'
    plt.rcParams['axes.unicode_minus'] = False
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    plt = None

logger = logging.getLogger(__name__)

_KO_CATEGORY = {
    'marketplace':              '온라인 마켓플레이스',
    'social_communication':     '소셜 / 커뮤니케이션',
    'blog':                     '블로그',
    'news':                     '뉴스 / 미디어',
    'product_promotion':        '제품·서비스 홍보',
    'documentation':            '기술 문서 / 위키',
    'authentication_required':  '로그인 필수',
    'forum':                    '포럼 / 토론',
    'social_network':           '소셜 네트워크',
    'communication':            '통신 / 메시지',
    'personal_blog':            '개인 블로그',
    'marketplace_forum_mixed':  '마켓 + 포럼 혼합',
    'unknown':                  '분류 불가',
}

_KO_CODA = {
    'Arms':        '무기 거래',
    'Crypto':      '암호화폐',
    'Drugs':       '마약',
    'Electronic':  '전자기기',
    'Financial':   '금융 사기',
    'Gambling':    '도박',
    'Hacking':     '해킹',
    'Porn':        '성인물',
    'Violence':    '폭력',
}


CSS = """
<style>
@import url("https://cdn.jsdelivr.net/gh/orioncactus/pretendard/dist/web/static/pretendard.css");
* { margin: 0; padding: 0; box-sizing: border-box; }
:root {
    --bg: #060714; --line: rgba(255,255,255,0.09); --text: #f1f5f9;
    --sub: #94a3b8; --muted: #64748b; --primary: #6366f1;
    --purple: #8b5cf6; --pink: #ec4899; --cyan: #22d3ee;
    --green: #10b981; --amber: #f59e0b;
    --danger: #ef4444; --success: #22c55e;
    --card-bg: rgba(15,20,40,0.92);
}
body {
    font-family: "Pretendard", sans-serif;
    background:
        radial-gradient(ellipse at 12% 5%,  rgba(99,102,241,0.22), transparent 38%),
        radial-gradient(ellipse at 90% 15%, rgba(236,72,153,0.16), transparent 34%),
        radial-gradient(ellipse at 65% 95%, rgba(34,211,238,0.12), transparent 36%),
        linear-gradient(160deg, #060714 0%, #0a0f1e 50%, #080914 100%);
    color: var(--text); min-height: 100vh; padding: 36px;
    overflow-x: hidden;
}
body::before {
    content: ""; position: fixed; inset: 0;
    background-image:
        linear-gradient(rgba(255,255,255,0.028) 1px, transparent 1px),
        linear-gradient(90deg, rgba(255,255,255,0.028) 1px, transparent 1px);
    background-size: 52px 52px;
    mask-image: linear-gradient(to bottom, rgba(0,0,0,0.7) 0%, transparent 75%);
    pointer-events: none; z-index: 0;
    animation: gridDrift 24s linear infinite;
}
.wrapper { position: relative; z-index: 1; max-width: 1060px; margin: 0 auto; animation: pageEnter 0.6s ease both; }

/* ── Header ── */
.header-bar { display: flex; align-items: center; gap: 16px; margin-bottom: 36px; }
.logo {
    width: 48px; height: 48px; border-radius: 10px; flex-shrink: 0;
    background: linear-gradient(135deg, var(--primary), var(--purple));
    position: relative; box-shadow: 0 0 32px rgba(99,102,241,0.35);
    animation: logoPulse 3.2s ease-in-out infinite;
}
.logo::after { content: ""; position: absolute; inset: 12px; border: 2.5px solid rgba(255,255,255,0.9); border-radius: 4px; }
.header-text .brand { font-weight: 800; font-size: 16px; letter-spacing: -0.2px; }
.header-text .system { font-size: 10px; font-weight: 700; letter-spacing: 0.22em; color: #6b7280; margin-top: 2px; }

/* ── Domain hero ── */
.hero {
    background: linear-gradient(135deg, rgba(17,24,39,0.97) 0%, rgba(13,19,36,0.99) 100%);
    border: 1px solid rgba(99,102,241,0.18);
    border-radius: 20px; padding: 44px 52px; margin-bottom: 28px;
    box-shadow: 0 24px 80px rgba(0,0,0,0.5), inset 0 1px 0 rgba(255,255,255,0.05);
    backdrop-filter: blur(20px); animation: heroEnter 0.7s ease both;
    position: relative; overflow: hidden;
}
.hero::before {
    content: ""; position: absolute; top: -60px; right: -60px;
    width: 260px; height: 260px; border-radius: 50%;
    background: radial-gradient(circle, rgba(99,102,241,0.12), transparent 70%);
    pointer-events: none;
}
.hero-label { font-size: 11px; font-weight: 700; letter-spacing: 0.22em; color: #818cf8; margin-bottom: 14px; }
.hero-domain { font-size: 34px; font-weight: 900; letter-spacing: -1px; color: #f8fafc; word-break: break-all; margin-bottom: 8px; }
.hero-meta { font-size: 14px; color: var(--muted); }

/* ── Summary strip ── */
.summary-strip {
    display: grid; grid-template-columns: repeat(4, 1fr); gap: 14px; margin-bottom: 28px;
    animation: fadeUp 0.7s ease both; animation-delay: 0.1s;
}
.stat-card {
    background: var(--card-bg); border: 1px solid var(--line);
    border-radius: 14px; padding: 22px 20px; text-align: center;
    transition: 0.22s ease; position: relative; overflow: hidden;
}
.stat-card::after {
    content: ""; position: absolute; bottom: 0; left: 0; right: 0; height: 3px;
    border-radius: 0 0 14px 14px;
}
.stat-card.accent-indigo::after  { background: linear-gradient(90deg, var(--primary), var(--purple)); }
.stat-card.accent-cyan::after    { background: linear-gradient(90deg, var(--cyan), #38bdf8); }
.stat-card.accent-green::after   { background: linear-gradient(90deg, var(--green), #34d399); }
.stat-card.accent-red::after     { background: linear-gradient(90deg, var(--danger), #f87171); }
.stat-card:hover { transform: translateY(-3px); border-color: rgba(255,255,255,0.16); box-shadow: 0 12px 36px rgba(0,0,0,0.35); }
.stat-value { font-size: 22px; font-weight: 900; color: #f1f5f9; margin-bottom: 5px; }
.stat-label { font-size: 11px; font-weight: 700; letter-spacing: 0.1em; color: var(--muted); }

/* ── Section card ── */
.section {
    background: var(--card-bg); border: 1px solid var(--line);
    border-radius: 16px; padding: 32px 36px; margin-bottom: 18px;
    box-shadow: 0 8px 32px rgba(0,0,0,0.28);
    backdrop-filter: blur(16px); animation: fadeUp 0.6s ease both;
    transition: 0.22s ease;
}
.section:hover { box-shadow: 0 14px 48px rgba(0,0,0,0.38); }
.section-header {
    display: flex; align-items: center; gap: 12px;
    margin-bottom: 22px; padding-bottom: 16px;
    border-bottom: 1px solid var(--line);
}
.section-icon {
    width: 36px; height: 36px; border-radius: 9px;
    display: flex; align-items: center; justify-content: center;
    font-size: 16px; flex-shrink: 0;
}
.icon-ai      { background: rgba(99,102,241,0.15); }
.icon-access  { background: rgba(34,211,238,0.12); }
.icon-index   { background: rgba(139,92,246,0.15); }
.icon-cat     { background: rgba(16,185,129,0.12); }
.icon-coda    { background: rgba(239,68,68,0.12); }
.section-title { font-size: 16px; font-weight: 800; color: #e2e8f0; letter-spacing: -0.2px; }
.section-subtitle { font-size: 12px; color: var(--muted); margin-top: 1px; }

/* ── Info grid ── */
.info-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; }
.info-grid.cols-3 { grid-template-columns: repeat(3, 1fr); }
.info-box {
    background: rgba(8,12,28,0.7); border: 1px solid var(--line);
    border-radius: 10px; padding: 16px 18px; transition: 0.2s ease;
}
.info-box:hover { border-color: rgba(99,102,241,0.3); background: rgba(99,102,241,0.06); }
.info-box.full { grid-column: 1 / -1; }
.info-key { font-size: 10px; font-weight: 700; letter-spacing: 0.14em; color: #818cf8; margin-bottom: 7px; text-transform: uppercase; }
.info-val { font-size: 14px; color: #cbd5e1; word-break: break-all; line-height: 1.5; }
.info-val.large { font-size: 24px; font-weight: 900; color: #f1f5f9; }
.info-val.code { font-family: 'Consolas', monospace; font-size: 12px; }

/* ── Badges ── */
.badge { display: inline-flex; align-items: center; gap: 5px; padding: 4px 12px; border-radius: 999px; font-size: 11px; font-weight: 800; letter-spacing: 0.05em; }
.badge-success { background: rgba(16,185,129,0.12); border: 1px solid rgba(16,185,129,0.25); color: #6ee7b7; }
.badge-danger  { background: rgba(239,68,68,0.12);  border: 1px solid rgba(239,68,68,0.25);  color: #fca5a5; }
.badge-warning { background: rgba(245,158,11,0.12); border: 1px solid rgba(245,158,11,0.25); color: #fcd34d; }
.badge-info    { background: rgba(34,211,238,0.1);  border: 1px solid rgba(34,211,238,0.2);  color: #67e8f9; }
.badge-neutral { background: rgba(100,116,139,0.15); border: 1px solid rgba(100,116,139,0.25); color: #94a3b8; }
.badge::before { content: ""; width: 5px; height: 5px; border-radius: 50%; background: currentColor; flex-shrink: 0; }

/* ── Table ── */
.data-table { width: 100%; border-collapse: collapse; margin-top: 4px; }
.data-table th, .data-table td { padding: 13px 16px; text-align: left; border-bottom: 1px solid rgba(255,255,255,0.055); font-size: 13px; }
.data-table thead tr { background: rgba(99,102,241,0.12); }
.data-table th { font-weight: 800; color: #c7d2fe; font-size: 11px; letter-spacing: 0.1em; text-transform: uppercase; }
.data-table td { color: #cbd5e1; }
.data-table tbody tr:last-child td { border-bottom: none; }
.data-table tbody tr:hover { background: rgba(99,102,241,0.06); }

/* ── Chart ── */
.chart-wrap {
    margin-top: 18px; border-radius: 12px; overflow: hidden;
    border: 1px solid var(--line);
    background: rgba(8,12,28,0.7);
}
.chart-label { font-size: 11px; font-weight: 700; letter-spacing: 0.12em; color: var(--muted); padding: 14px 18px 0; text-transform: uppercase; }
.chart-wrap img { display: block; width: 100%; height: auto; padding: 8px 12px 12px; }

/* ── Warn box ── */
.warn-box {
    background: rgba(245,158,11,0.07); border: 1px solid rgba(245,158,11,0.2);
    border-left: 4px solid var(--amber); border-radius: 10px;
    padding: 16px 18px; color: #fde68a; font-size: 14px; margin-top: 14px;
}

/* ── Scrollable list ── */
.scroll-list { max-height: 200px; overflow-y: auto; padding-right: 4px; }
.scroll-list::-webkit-scrollbar { width: 4px; }
.scroll-list::-webkit-scrollbar-track { background: transparent; }
.scroll-list::-webkit-scrollbar-thumb { background: rgba(99,102,241,0.4); border-radius: 4px; }
.url-item { font-size: 12px; color: #818cf8; font-family: monospace; padding: 3px 0; border-bottom: 1px solid rgba(255,255,255,0.04); }
.url-item:last-child { border-bottom: none; }

/* ── Footer ── */
.footer { margin-top: 44px; padding: 20px 0; border-top: 1px solid var(--line); display: flex; align-items: center; justify-content: space-between; }
.footer-brand { font-size: 13px; font-weight: 700; color: var(--muted); }
.footer-ts { font-size: 12px; color: #374151; }

/* ── Animations ── */
@keyframes pageEnter { from { opacity:0; transform: translateY(20px) scale(0.988); } to { opacity:1; transform: none; } }
@keyframes heroEnter { from { opacity:0; transform: translateY(28px); } to { opacity:1; transform: none; } }
@keyframes fadeUp    { from { opacity:0; transform: translateY(16px); } to { opacity:1; transform: none; } }
@keyframes gridDrift { from { background-position: 0 0; } to { background-position: 52px 52px; } }
@keyframes logoPulse { 0%,100% { box-shadow: 0 0 18px rgba(99,102,241,0.2); } 50% { box-shadow: 0 0 40px rgba(99,102,241,0.5); } }

@media (max-width: 768px) {
    body { padding: 18px; }
    .hero { padding: 28px 24px; }
    .hero-domain { font-size: 22px; }
    .summary-strip { grid-template-columns: repeat(2, 1fr); }
    .section { padding: 22px 20px; }
    .info-grid, .info-grid.cols-3 { grid-template-columns: 1fr; }
}
</style>
"""


class AgentReportGenerator:
    """단일 도메인 분석 결과 HTML 보고서 생성"""

    def __init__(self, output_dir: str = "analysis_reports"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        logger.info(f"보고서 저장 경로: {self.output_dir}")

    def generate_report(self, domain: str, analysis_result: Dict,
                        coda_result: Dict = None,
                        llm_result: Dict = None, category_result: Dict = None) -> str:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"analysis_report_{domain.replace('.', '_')}_{timestamp}.html"
        filepath = self.output_dir / filename

        charts = self._generate_charts(coda_result or {}, category_result or {})
        html_content = self._generate_html(
            domain=domain,
            analysis_result=analysis_result,
            coda_result=coda_result or {},
            charts=charts,
            llm_result=llm_result or {},
            category_result=category_result or {}
        )

        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(html_content)

        logger.info(f"✅ 보고서 생성 완료: {filepath}")
        return str(filepath)

    def _extract_onion_domains(self, html_content: str, limit: int = 10) -> list:
        try:
            matches = re.findall(r'([a-z0-9\-]+\.onion)', html_content, re.IGNORECASE)
            unique = sorted(set(m.lower() for m in matches))
            return unique[:limit]
        except Exception:
            return []

    def _generate_charts(self, coda_result: Dict, category_result: Dict) -> Dict:
        charts = {}
        if not MATPLOTLIB_AVAILABLE:
            return charts
        try:
            if coda_result.get('all_probs'):
                charts['coda'] = self._create_bar_chart(
                    data=coda_result['all_probs'],
                    title='CoDA 범죄 카테고리 확률',
                    color='#ef4444'
                )
            cat_scores = category_result.get('category_scores', {})
            if cat_scores:
                ko_scores = {_KO_CATEGORY.get(k, k.replace('_', ' ').title()): v for k, v in cat_scores.items()}
                charts['category'] = self._create_bar_chart(
                    data=ko_scores,
                    title='사이트 유형 분류 점수',
                    color='#6366f1'
                )
        except Exception as e:
            logger.error(f"차트 생성 오류: {e}")
        return charts

    def _save_chart(self, fig) -> str:
        buf = BytesIO()
        plt.savefig(buf, format='png', bbox_inches='tight', dpi=130,
                    facecolor='#080c1c', edgecolor='none')
        buf.seek(0)
        result = base64.b64encode(buf.read()).decode('utf-8')
        plt.close(fig)
        return result

    def _create_bar_chart(self, data: Dict, title: str, color: str) -> str:
        try:
            labels = list(data.keys())
            values = [v * 100 if v <= 1.0 else float(v) for v in data.values()]

            fig_h = max(3.2, len(labels) * 0.65)
            fig, ax = plt.subplots(figsize=(8.5, fig_h))
            fig.patch.set_facecolor('#080c1c')
            ax.set_facecolor('#080c1c')

            # gradient-like bars using alpha variation
            bar_colors = [color] * len(labels)
            bars = ax.barh(labels, values, color=bar_colors, alpha=0.78,
                           height=0.52, edgecolor='none')

            # value labels
            for bar, val in zip(bars, values):
                ax.text(bar.get_width() + 1.2, bar.get_y() + bar.get_height() / 2,
                        f'{val:.1f}%', va='center', ha='left',
                        color='#e2e8f0', fontsize=10, fontweight='bold')

            ax.set_xlim(0, 118)
            ax.set_xlabel('%', color='#64748b', fontsize=10, fontweight='bold')
            ax.set_title(title, color='#e2e8f0', fontsize=12, fontweight='bold', pad=14)

            ax.set_yticks(range(len(labels)))
            ax.set_yticklabels(labels, fontsize=10, fontweight='bold', color='#cbd5e1')
            x_ticks = ax.get_xticks()
            ax.set_xticks(x_ticks)
            ax.set_xticklabels([f'{int(t)}' for t in x_ticks],
                               fontsize=9, fontweight='bold', color='#64748b')

            for spine in ('top', 'right'):
                ax.spines[spine].set_visible(False)
            ax.spines['bottom'].set_color('#1e293b')
            ax.spines['left'].set_color('#1e293b')
            ax.xaxis.grid(True, color='#1e293b', linewidth=0.6, linestyle='--')
            ax.set_axisbelow(True)

            fig.tight_layout(pad=1.2)
            return self._save_chart(fig)
        except Exception as e:
            logger.error(f"막대 차트 오류: {e}")
            return ""

    def _generate_html(self, domain: str, analysis_result: Dict,
                       coda_result: Dict, charts: Dict, llm_result: Dict,
                       category_result: Dict = None) -> str:
        accessibility = analysis_result.get('accessibility', {})
        indexing = analysis_result.get('indexing', {})
        html_collected = analysis_result.get('html_collected', False)
        category_result = category_result or {}

        def badge(ok, yes='접근 가능', no='접근 불가'):
            cls = 'badge-success' if ok else 'badge-danger'
            return f'<span class="badge {cls}">{yes if ok else no}</span>'

        now = datetime.now()
        ts  = now.strftime('%Y년 %m월 %d일 %H:%M:%S')
        ts_f = now.strftime('%Y-%m-%d %H:%M:%S')

        # ── summary strip values
        is_accessible = accessibility.get('is_accessible', False)
        access_label  = '접근 가능' if is_accessible else '접근 불가'
        access_cls    = 'accent-cyan'

        risk = (llm_result or {}).get('risk_level', '—') if llm_result and llm_result.get('success') else '—'
        risk_cls_map  = {'낮음': 'accent-green', '중간': 'accent-indigo', '높음': 'accent-red', '매우높음': 'accent-red'}
        risk_strip_cls = risk_cls_map.get(risk, 'accent-indigo')

        raw_primary = category_result.get('primary_category', '')
        primary_ko  = _KO_CATEGORY.get(raw_primary, raw_primary.replace('_', ' ').title()) if raw_primary else '—'

        coda_cat = coda_result.get('category', '').title() if coda_result.get('available') else '—'
        coda_ko  = '—' if risk == '낮음' else (_KO_CODA.get(coda_cat, coda_cat) if coda_cat != '—' else '—')

        h = f"""<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>분석 보고서 — {domain}</title>
{CSS}
</head>
<body>
<div class="wrapper">

<div class="header-bar">
  <div class="logo"></div>
  <div class="header-text">
    <div class="brand">Darkweb Analyzer</div>
    <div class="system">SECURITY ANALYSIS SYSTEM</div>
  </div>
</div>

<div class="hero">
  <div class="hero-label">ANALYSIS REPORT</div>
  <div class="hero-domain">{domain}</div>
  <div class="hero-meta">생성 일시: {ts}</div>
</div>
"""

        # ── warning
        if analysis_result.get('analysis_warning'):
            h += f'<div class="warn-box">&#9888; {analysis_result["analysis_warning"]}</div>'

        # ── summary strip
        h += f"""
<div class="summary-strip">
  <div class="stat-card {access_cls}">
    <div class="stat-value">{access_label}</div>
    <div class="stat-label">접근성</div>
  </div>
  <div class="stat-card {risk_strip_cls}">
    <div class="stat-value">{risk}</div>
    <div class="stat-label">AI 위험도</div>
  </div>
  <div class="stat-card accent-green">
    <div class="stat-value" style="font-size:16px;">{primary_ko}</div>
    <div class="stat-label">사이트 유형</div>
  </div>
  <div class="stat-card accent-red">
    <div class="stat-value" style="font-size:16px;">{coda_ko}</div>
    <div class="stat-label">CoDA 분류</div>
  </div>
</div>
"""

        # ── AI 분석
        if llm_result and llm_result.get('success'):
            risk_badge_cls = {'낮음': 'badge-success', '중간': 'badge-warning', '높음': 'badge-danger', '매우높음': 'badge-danger'}.get(risk, 'badge-neutral')
            features = llm_result.get('notable_features', [])
            feat_html = ('<ul style="list-style:none;margin-top:8px;">' +
                         ''.join(f'<li style="padding:4px 0;font-size:13px;color:#94a3b8;border-bottom:1px solid rgba(255,255,255,0.05);">'
                                 f'<span style="color:#818cf8;margin-right:8px;">&#9656;</span>{f}</li>' for f in features) +
                         '</ul>') if features else ''
            h += f"""
<div class="section" style="animation-delay:0.15s">
  <div class="section-header">
    <div class="section-icon icon-ai">&#129302;</div>
    <div>
      <div class="section-title">AI 사이트 분석</div>
      <div class="section-subtitle">모델: {llm_result.get('model_used','')}</div>
    </div>
  </div>
  <div class="info-grid cols-3">
    <div class="info-box"><div class="info-key">사이트 유형</div><div class="info-val">{llm_result.get('site_type') or '—'}</div></div>
    <div class="info-box"><div class="info-key">주요 언어</div><div class="info-val">{llm_result.get('language') or '—'}</div></div>
    <div class="info-box"><div class="info-key">AI 위험도</div><div class="info-val"><span class="badge {risk_badge_cls}">{risk}</span></div></div>
    <div class="info-box full"><div class="info-key">목적</div><div class="info-val">{llm_result.get('purpose') or '—'}</div></div>
    <div class="info-box full"><div class="info-key">요약</div><div class="info-val">{llm_result.get('summary') or '—'}</div></div>
    <div class="info-box full"><div class="info-key">위험도 근거</div><div class="info-val">{llm_result.get('risk_reason') or '—'}</div></div>
    {f'<div class="info-box full"><div class="info-key">주목할 특징</div><div class="info-val">{feat_html}</div></div>' if feat_html else ''}
  </div>
</div>
"""
        elif llm_result and not llm_result.get('success'):
            err = llm_result.get('error', '')
            if err not in ('HTML 미수집', 'API 키 미설정'):
                h += f"""
<div class="section">
  <div class="section-header"><div class="section-icon icon-ai">&#129302;</div><div class="section-title">AI 사이트 분석</div></div>
  <div class="warn-box">분석 실패: {err}</div>
</div>
"""

        # ── 접근성
        response_time = round(accessibility.get('response_time') or 0, 2)
        status_code   = accessibility.get('status_code', 'N/A')
        h += f"""
<div class="section" style="animation-delay:0.2s">
  <div class="section-header">
    <div class="section-icon icon-access">&#127760;</div>
    <div><div class="section-title">접근성 정보</div><div class="section-subtitle">Tor 네트워크 경유 접속 결과</div></div>
  </div>
  <div class="info-grid cols-3">
    <div class="info-box"><div class="info-key">접근 상태</div><div class="info-val">{badge(is_accessible)}</div></div>
    <div class="info-box"><div class="info-key">HTTP 상태코드</div><div class="info-val large">{status_code}</div></div>
    <div class="info-box"><div class="info-key">HTML 수집</div><div class="info-val">{badge(html_collected, '수집됨', '미수집')}</div></div>
"""
        if accessibility.get('fallback_domain'):
            h += f'<div class="info-box"><div class="info-key">재검증 도메인</div><div class="info-val code">{accessibility["fallback_domain"]}</div></div>'
            h += f'<div class="info-box"><div class="info-key">재검증 결과</div><div class="info-val">{badge(accessibility.get("fallback_accessible"), "성공", "실패")}</div></div>'
        h += '  </div>\n</div>'

        # ── 검색 색인
        ahmia_b = badge(indexing.get('ahmia_found'), '색인됨', '미발견')
        ddgo_b  = badge(indexing.get('duckduckgo_found'), '색인됨', '미발견')
        extracted_urls  = indexing.get('extracted_urls', [])
        onion_in_page   = self._extract_onion_domains(analysis_result.get('html_content', '')) if html_collected else []
        h += f"""
<div class="section" style="animation-delay:0.25s">
  <div class="section-header">
    <div class="section-icon icon-index">&#128269;</div>
    <div><div class="section-title">검색 색인 정보</div><div class="section-subtitle">다크웹 검색엔진 노출 현황</div></div>
  </div>
  <table class="data-table">
    <thead><tr><th>검색 엔진</th><th>상태</th><th>결과 수</th></tr></thead>
    <tbody>
      <tr><td>Ahmia</td><td>{ahmia_b}</td><td>{indexing.get('ahmia_results', 0)}건</td></tr>
      <tr><td>DuckDuckGo</td><td>{ddgo_b}</td><td>{'감지됨' if indexing.get('duckduckgo_found') else '미감지'}</td></tr>
    </tbody>
  </table>
"""
        if extracted_urls:
            urls_items = ''.join(f'<div class="url-item">{u}</div>' for u in extracted_urls[:20])
            h += f'<div class="info-box full" style="margin-top:14px;"><div class="info-key">상대 경로 ({len(extracted_urls)}개)</div><div class="scroll-list" style="margin-top:6px;">{urls_items}</div></div>'
        if onion_in_page:
            onion_items = ''.join(f'<div class="url-item">{d}</div>' for d in onion_in_page)
            h += f'<div class="info-box full" style="margin-top:10px;"><div class="info-key">페이지 내 .onion 도메인 ({len(onion_in_page)}개)</div><div class="scroll-list" style="margin-top:6px;">{onion_items}</div></div>'
        h += '</div>'

        # ── 사이트 유형 분류
        h += """
<div class="section" style="animation-delay:0.3s">
  <div class="section-header">
    <div class="section-icon icon-cat">&#128203;</div>
    <div><div class="section-title">사이트 유형 분류</div><div class="section-subtitle">BART 제로샷 분류 모델</div></div>
  </div>
"""
        if not html_collected:
            h += '<div class="warn-box">HTML 미수집으로 분류를 수행하지 않았습니다.</div>'
        elif category_result.get('skip_reason'):
            h += '<div class="info-box"><div class="info-val">분류 스킵됨</div></div>'
        else:
            raw_secondary = category_result.get('secondary_category')
            secondary_ko  = _KO_CATEGORY.get(raw_secondary, raw_secondary.replace('_', ' ').title()) if raw_secondary else None
            conf = round(category_result.get('confidence', 0) * 100, 1)
            h += f"""
  <div class="info-grid">
    <div class="info-box"><div class="info-key">주요 유형</div><div class="info-val large">{primary_ko}</div></div>
    <div class="info-box"><div class="info-key">신뢰도</div><div class="info-val large">{conf}<span style="font-size:14px;font-weight:500;color:var(--sub);">%</span></div></div>
    {f'<div class="info-box full"><div class="info-key">보조 유형</div><div class="info-val">{secondary_ko}</div></div>' if secondary_ko else ''}
  </div>
"""
            if charts.get('category'):
                h += f'<div class="chart-wrap"><div class="chart-label">카테고리별 점수 분포</div><img src="data:image/png;base64,{charts["category"]}" alt="사이트 유형 차트"></div>'
        h += '</div>'

        # ── CoDA 분류
        if risk != '낮음':  # 위험도가 '낮음'이 아닐 때만 표시
            h += """
<div class="section" style="animation-delay:0.35s">
  <div class="section-header">
    <div class="section-icon icon-coda">&#9888;</div>
    <div><div class="section-title">CoDA 범죄 카테고리 분류</div><div class="section-subtitle">DarkBERT 기반 범죄 콘텐츠 분류</div></div>
  </div>
"""
            if not html_collected:
                h += '<div class="warn-box">HTML 미수집으로 분류를 수행하지 않았습니다.</div>'
            elif not coda_result.get('available'):
                h += '<div class="warn-box">&#9888; 학습된 모델 없음 — <code>python3 analyzers/train_coda_classifier.py</code> 실행 필요</div>'
            else:
                raw_cat   = coda_result.get('category', 'unknown').title()
                coda_ko_s = _KO_CODA.get(raw_cat, raw_cat)
                conf_c    = round(coda_result.get('confidence', 0) * 100, 1)
                uncertain = coda_result.get('uncertain', False)

                h += f"""
  <div class="info-grid">
    <div class="info-box"><div class="info-key">분류 결과</div><div class="info-val large">{coda_ko_s}</div></div>
    <div class="info-box"><div class="info-key">신뢰도</div><div class="info-val large">{conf_c}<span style="font-size:14px;font-weight:500;color:var(--sub);">%</span></div></div>
    {('<div class="info-box full"><div class="info-key">주의</div><div class="info-val"><span class="badge badge-warning">분류 불확실</span> 결과 해석 시 주의 필요</div></div>') if uncertain else ''}
  </div>
"""
                all_probs = coda_result.get('all_probs', {})
                if all_probs:
                    top5 = list(all_probs.items())[:5]
                    prob_items = ' &nbsp;·&nbsp; '.join(
                        f'<b style="color:#e2e8f0;">{_KO_CODA.get(k.title(), k)}</b> <span style="color:#818cf8;">{round(v*100,1)}%</span>'
                        for k, v in top5
                    )
                    h += f'<div class="info-box full" style="margin-top:12px;"><div class="info-key">상위 확률 분포</div><div class="info-val" style="font-size:13px;line-height:2;">{prob_items}</div></div>'
                if charts.get('coda'):
                    h += f'<div class="chart-wrap"><div class="chart-label">범죄 카테고리별 확률 분포</div><img src="data:image/png;base64,{charts["coda"]}" alt="CoDA 차트"></div>'
            h += '</div>'

        h += f"""
<div class="footer">
  <div class="footer-brand">Darkweb Analyzer</div>
  <div class="footer-ts">자동 생성 &nbsp;|&nbsp; {ts_f}</div>
</div>

</div>
</body></html>
"""
        return h
