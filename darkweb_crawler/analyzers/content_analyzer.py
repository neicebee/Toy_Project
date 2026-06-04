"""
HTML 콘텐츠 분석기 - DarkBERT + CoDA 센트로이드 기반 범죄 카테고리 분류
"""

import re
import logging
import importlib
import os

from utils.html_cleaner import HTMLCleaner
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np

logger = logging.getLogger(__name__)

# CoDA 카테고리명 → 분석기 카테고리명 매핑
CODA_CATEGORY_MAP = {
    'Arms': 'arms', 'arms': 'arms',
    'Drugs': 'drugs', 'drugs': 'drugs',
    'Violence': 'violence', 'violence': 'violence',
    'Financial': 'financial', 'financial': 'financial',
    'Fraud': 'financial', 'fraud': 'financial',
    'Counterfeit': 'financial', 'counterfeit': 'financial',
    'Hacking': 'hacking', 'hacking': 'hacking',
    'Gambling': 'gambling', 'gambling': 'gambling',
    'Electronic': 'electronic', 'electronic': 'electronic',
    'Crypto': 'crypto', 'crypto': 'crypto', 'Bitcoin': 'crypto',
    'Porn': 'porn', 'porn': 'porn',
    'Others': 'others', 'others': 'others',
}


class ContentAnalyzer:
    """HTML 콘텐츠를 분석하여 불법성 판정 (DarkBERT + 키워드 혼합)"""

    def __init__(self, keywords_config: Dict = None,
                 use_darkbert: bool = True,
                 darkbert_model_name: str = 's2w-ai/darkbert',
                 centroids_path: str = None,
                 rule_weight: float = 0.5,
                 darkbert_weight: float = 0.5):
        self.keywords_config = keywords_config or self._default_keywords()
        self.use_darkbert = use_darkbert
        self.darkbert_model_name = darkbert_model_name
        self.centroids_path = centroids_path or self._default_centroids_path()
        self.rule_weight = rule_weight
        self.darkbert_weight = darkbert_weight
        self.max_len = 512

        self._tokenizer = None
        self._model = None
        self._darkbert_loaded = False
        self._centroids: Dict[str, np.ndarray] = {}
        self._centroids_loaded = False

        self.html_cleaners = [
            r'<script[^>]*>.*?</script>',
            r'<style[^>]*>.*?</style>',
            r'<[^>]+>',
        ]
        self._validate_weights()

    def _default_centroids_path(self) -> str:
        base = Path(__file__).parent.parent
        candidates = [
            base / 'data' / 'coda_centroids.csv',
            base / 'data' / 'coda_centroids_full.csv',
            Path.home() / 'Downloads' / 'coda_centroids_colab.csv',
        ]
        for p in candidates:
            if p.exists():
                logger.info(f"센트로이드 파일 발견: {p}")
                return str(p)
        return str(candidates[0])

    def _validate_weights(self):
        total = self.rule_weight + self.darkbert_weight
        if total <= 0:
            self.rule_weight = 1.0
            self.darkbert_weight = 0.0
            return
        self.rule_weight = self.rule_weight / total
        self.darkbert_weight = self.darkbert_weight / total

    # ──────────────────────────────────────────────
    # DarkBERT 로딩
    # ──────────────────────────────────────────────

    def _load_darkbert(self):
        if self._darkbert_loaded:
            return self._tokenizer, self._model

        self._darkbert_loaded = True

        if not self.use_darkbert:
            return None, None

        try:
            torch = importlib.import_module('torch')
            transformers = importlib.import_module('transformers')

            logger.info("🔄 DarkBERT 로딩 중...")
            tokenizer = transformers.AutoTokenizer.from_pretrained(self.darkbert_model_name)
            model = transformers.AutoModel.from_pretrained(self.darkbert_model_name)

            special_tokens = {
                'additional_special_tokens': ['[NUM]', '[TIME]', '[CRYPTO]', '[URL]', '[TOKEN]']
            }
            tokenizer.add_special_tokens(special_tokens)
            model.resize_token_embeddings(len(tokenizer))
            model.eval()

            if torch.backends.mps.is_available():
                device = torch.device('mps')
            elif torch.cuda.is_available():
                device = torch.device('cuda')
            else:
                device = torch.device('cpu')

            model = model.to(device)
            self._tokenizer = tokenizer
            self._model = model
            self._device = device
            logger.info(f"✅ DarkBERT 로드 완료 (device={device})")
        except Exception as e:
            logger.error(f"❌ DarkBERT 로드 실패: {e}")
            logger.warning("   키워드 기반만 사용합니다")
            self._tokenizer = None
            self._model = None

        return self._tokenizer, self._model

    # ──────────────────────────────────────────────
    # 센트로이드 로딩
    # ──────────────────────────────────────────────

    def _load_centroids(self) -> Dict[str, np.ndarray]:
        if self._centroids_loaded:
            return self._centroids

        self._centroids_loaded = True

        if not os.path.exists(self.centroids_path):
            logger.warning(f"⚠️ 센트로이드 파일 없음: {self.centroids_path}")
            return {}

        try:
            pd = importlib.import_module('pandas')
            df = pd.read_csv(self.centroids_path)

            vec_cols = [c for c in df.columns if c.startswith('v_')]
            for _, row in df.iterrows():
                coda_cat = str(row['카테고리'])
                analyzer_cat = CODA_CATEGORY_MAP.get(coda_cat, coda_cat.lower())
                self._centroids[analyzer_cat] = row[vec_cols].values.astype(np.float32)

            logger.info(f"✅ 센트로이드 로드 완료: {list(self._centroids.keys())}")
        except Exception as e:
            logger.error(f"❌ 센트로이드 로드 실패: {e}")

        return self._centroids

    # ──────────────────────────────────────────────
    # 텍스트 정규화 (CoDA 전처리와 동일)
    # ──────────────────────────────────────────────

    def _normalize_for_darkbert(self, text: str) -> str:
        """
        CoDA finalize_data.py 전처리와 동일한 방식으로 정규화.

        순서 중요: 대소문자 패턴(BTC 주소 등)을 먼저 치환 → 소문자 변환 → 특수문자 제거.
        소문자 변환을 먼저 하면 Base58 Bitcoin 주소 regex가 작동하지 않음.

        CoDA는 id_number 토큰만 [NUM]으로 변환했고 모든 숫자를 치환하지 않았으므로
        가격/수량 등 일반 숫자는 그대로 유지한다 (finalize_data.py와 동일).
        """
        # 1단계: 대소문자 구분이 필요한 패턴 먼저 치환
        # 암호화폐 주소 → [CRYPTO]  (CoDA: id_btc_address, id_eth_address 등)
        text = re.sub(r'\b[13][a-km-zA-HJ-NP-Z1-9]{25,34}\b', '[CRYPTO]', text)  # BTC
        text = re.sub(r'\b0x[a-fA-F0-9]{40}\b', '[CRYPTO]', text)                # ETH
        text = re.sub(r'\b4[0-9AB][1-9A-HJ-NP-Za-km-z]{93}\b', '[CRYPTO]', text) # Monero

        # URL → [URL]  (CoDA: id_normal_url, id_onion_url)
        text = re.sub(r'https?://\S+', '[URL]', text)
        text = re.sub(r'\b\S+\.onion\b\S*', '[URL]', text)

        # 2단계: 소문자 변환 (finalize_data.py 1번 단계) — [CRYPTO], [URL] → [crypto], [url]
        text = text.lower()

        # 3단계: 특수문자 제거 (영문/숫자/공백/괄호만 유지) — finalize_data.py와 동일
        # 이 시점에서 [crypto], [url] 등은 소문자라 생존함
        text = re.sub(r'[^a-z0-9\s\[\]]', '', text)

        return text.strip()

    # ──────────────────────────────────────────────
    # DarkBERT 임베딩 (512토큰 청크 슬라이딩)
    # ──────────────────────────────────────────────

    def _get_darkbert_vector(self, text: str):
        tokenizer, model = self._load_darkbert()
        if tokenizer is None or model is None:
            return None

        try:
            torch = importlib.import_module('torch')
            inputs = tokenizer(text, truncation=False, return_tensors='pt')
            input_ids = inputs['input_ids'][0]

            chunk_vectors = []
            for i in range(0, len(input_ids), self.max_len):
                chunk = input_ids[i:i + self.max_len].unsqueeze(0).to(self._device)
                mask = torch.ones_like(chunk).to(self._device)
                with torch.no_grad():
                    outputs = model(input_ids=chunk, attention_mask=mask)
                    chunk_vec = outputs.last_hidden_state.mean(dim=1)
                    chunk_vectors.append(chunk_vec.cpu().numpy())

            if not chunk_vectors:
                return np.zeros((1, 768), dtype=np.float32)
            return np.mean(chunk_vectors, axis=0)
        except Exception as e:
            logger.error(f"DarkBERT 임베딩 오류: {e}")
            return None

    def _cosine_similarity(self, a: np.ndarray, b: np.ndarray) -> float:
        a = a.flatten().astype(np.float32)
        b = b.flatten().astype(np.float32)
        norm_a = np.linalg.norm(a)
        norm_b = np.linalg.norm(b)
        if norm_a == 0 or norm_b == 0:
            return 0.0
        return float(np.dot(a, b) / (norm_a * norm_b))

    # ──────────────────────────────────────────────
    # 센트로이드 기반 카테고리 점수 계산
    # ──────────────────────────────────────────────

    def _get_page_vector(self, text: str):
        """
        CoDA 전처리(finalize_data.py)와 동일한 정규화 후 DarkBERT 임베딩.
        실제 암호화폐 주소/URL을 특수 토큰으로 치환하여 센트로이드와 분포를 맞춤.
        """
        normalized = self._normalize_for_darkbert(text)
        return self._get_darkbert_vector(normalized)

    def _calculate_darkbert_scores(self, text: str,
                                   cached_vec=None) -> Dict[str, float]:
        """
        CoDA 센트로이드와 코사인 유사도 기반 범죄 카테고리 점수 반환 (0~1).
        cached_vec: 이미 계산된 페이지 벡터가 있으면 재사용 (이중 임베딩 방지).
        """
        centroids = self._load_centroids()
        if not centroids:
            return {}

        vec = cached_vec if cached_vec is not None else self._get_page_vector(text)
        if vec is None:
            return {}

        scores = {}
        for category, centroid in centroids.items():
            sim = self._cosine_similarity(vec, centroid)
            scores[category] = float((sim + 1) / 2)  # (-1~1) → (0~1)

        if scores:
            best = max(scores, key=scores.get)
            logger.info(f"🎯 센트로이드 최근접 범죄 카테고리: {best} (유사도: {scores[best]:.4f})")

        return scores

    def classify_by_centroid(self, html: str, cached_vec=None,
                             temperature: float = 0.05) -> Dict:
        """
        CoDA 센트로이드와 코사인 유사도 + softmax로 범죄 카테고리 분류.

        raw cosine similarity는 값들이 0.51~0.55처럼 몰려 단순 argmax가 불안정하므로
        temperature scaling softmax로 상대적 차이를 증폭시킨다.
        temperature가 낮을수록 1위 카테고리가 더 뚜렷하게 분리됨 (기본값 0.1).
        """
        text = self.clean_html(html)
        if not text:
            return {'crime_category': 'unknown', 'confidence': 0.0,
                    'all_scores': {}, 'raw_cosine': {}}

        centroids = self._load_centroids()
        if not centroids:
            return {'crime_category': 'unknown', 'confidence': 0.0,
                    'all_scores': {}, 'raw_cosine': {}}

        vec = cached_vec if cached_vec is not None else self._get_page_vector(text)
        if vec is None:
            return {'crime_category': 'unknown', 'confidence': 0.0,
                    'all_scores': {}, 'raw_cosine': {}}

        # 1) 각 센트로이드와 raw 코사인 유사도 계산
        categories = list(centroids.keys())
        raw_sims = np.array([
            self._cosine_similarity(vec, centroids[c]) for c in categories
        ], dtype=np.float32)

        # 2) Temperature scaling softmax (수치 안정성: max 빼고 exp)
        scaled = raw_sims / temperature
        scaled -= scaled.max()
        exp_vals = np.exp(scaled)
        softmax_scores = exp_vals / exp_vals.sum()

        scores = {c: float(softmax_scores[i]) for i, c in enumerate(categories)}
        raw_cosine = {c: round(float(raw_sims[i]), 4) for i, c in enumerate(categories)}

        sorted_scores = sorted(scores.items(), key=lambda x: x[1], reverse=True)
        primary = sorted_scores[0][0]
        confidence = sorted_scores[0][1]

        logger.info(
            f"🎯 CoDA 범죄 카테고리: {primary.upper()} "
            f"(softmax={confidence:.4f}, raw_cos={raw_cosine[primary]:.4f})"
        )

        return {
            'crime_category': primary,
            'confidence': round(confidence, 4),
            'all_scores': {k: round(v, 4) for k, v in sorted_scores},
            'raw_cosine': raw_cosine,
        }

    # ──────────────────────────────────────────────
    # HTML 정제
    # ──────────────────────────────────────────────

    def clean_html(self, html: str) -> str:
        if not html:
            return ""
        text = html
        for pattern in self.html_cleaners:
            text = re.sub(pattern, ' ', text, flags=re.IGNORECASE | re.DOTALL)
        text = re.sub(r'\s+', ' ', text)
        return text.strip()

    # ──────────────────────────────────────────────
    # 키워드 추출
    # ──────────────────────────────────────────────

    def extract_keywords(self, text: str, category: str) -> Tuple[List[str], int]:
        if category not in self.keywords_config:
            return [], 0

        config = self.keywords_config[category]
        keywords = config if isinstance(config, list) else config.get('keywords', [])
        found, count = [], 0
        text_lower = text.lower()

        for kw in keywords:
            matches = re.findall(r'\b' + re.escape(kw) + r'\b', text_lower, re.IGNORECASE)
            if matches:
                found.append(kw)
                count += len(matches)

        return found, count

    # ──────────────────────────────────────────────
    # 메인 분석
    # ──────────────────────────────────────────────

    def detect_illegal_content(self, html: str, verbose: bool = False,
                               category_result: Dict = None) -> Dict:
        text = self.clean_html(html)

        if not text:
            logger.warning("분석할 텍스트가 없음")
            return {
                'is_illegal': False,
                'illegal_confidence': 0.0,
                'categories': {},
                'primary_illegal_type': None,
                'total_matches': 0,
                'error': 'No text to analyze'
            }

        # 페이지 벡터 1회만 계산 → classify_by_centroid에 재사용
        _page_vec = self._get_page_vector(text)
        darkbert_scores = self._calculate_darkbert_scores(text, cached_vec=_page_vec)
        category_results = {}
        total_matches = 0
        illegal_categories = []

        for category in self.keywords_config.keys():
            keywords, match_count = self.extract_keywords(text, category)

            config = self.keywords_config[category]
            kw_list = config if isinstance(config, list) else config.get('keywords', [])
            keyword_signal = min(match_count / len(kw_list), 1.0) if kw_list else 0.0

            darkbert_score = darkbert_scores.get(category, 0.0)
            combined_score = (keyword_signal * self.rule_weight) + (darkbert_score * self.darkbert_weight)

            # 불법 여부 판정은 키워드 기반으로만 한다.
            # darkbert_score는 (sim+1)/2 매핑 후 항상 0.65+ 이므로 임계값 기준이 무의미.
            # 카테고리 분류는 classify_by_centroid(softmax)가 별도로 담당.
            found = len(keywords) > 0
            severity = self._calculate_severity(category, match_count, len(text)) if found else 'none'

            category_results[category] = {
                'found': found,
                'keywords': keywords,
                'match_count': match_count,
                'severity': severity,
                'darkbert_score': round(darkbert_score, 4),
                'combined_score': round(combined_score, 4)
            }

            if found and severity in ['high', 'medium']:
                illegal_categories.append((category, severity, combined_score))

            total_matches += match_count

            if verbose and found:
                logger.info(
                    f"  [{category}] 키워드={match_count}회, "
                    f"DarkBERT={darkbert_score:.2f}, 종합={combined_score:.2f}, 심각도={severity}"
                )

        is_illegal = len(illegal_categories) > 0

        primary_illegal_type = None
        primary_illegal_score = 0.0
        if illegal_categories:
            illegal_categories.sort(key=lambda x: x[2], reverse=True)
            primary_illegal_type = illegal_categories[0][0]
            primary_illegal_score = float(illegal_categories[0][2])

        keyword_confidence = self._calculate_illegal_confidence(
            total_matches, len(text), len(illegal_categories)
        )
        darkbert_illegal_confidence = self._calculate_darkbert_illegal_confidence(darkbert_scores)
        confidence = primary_illegal_score if is_illegal else 0.0

        result = {
            'is_illegal': is_illegal,
            'illegal_confidence': confidence,
            'categories': category_results,
            'primary_illegal_type': primary_illegal_type,
            'primary_illegal_score': primary_illegal_score,
            'total_matches': total_matches,
            'keyword_confidence': keyword_confidence,
            'darkbert_confidence': darkbert_illegal_confidence,
            'darkbert_scores': darkbert_scores,
            'darkbert_model_used': self._model is not None,
            '_page_vec': _page_vec,  # classify_by_centroid 재사용용 (이중 임베딩 방지)
        }

        if verbose:
            logger.info(f"불법 콘텐츠 판정: 불법={is_illegal}, 신뢰도={confidence:.2f}, "
                        f"주요 카테고리={primary_illegal_type}")

        return result

    # ──────────────────────────────────────────────
    # 헬퍼
    # ──────────────────────────────────────────────

    def _severity_from_darkbert_score(self, score: float) -> str:
        if score >= 0.75:
            return 'high'
        if score >= 0.65:
            return 'medium'
        if score >= 0.60:
            return 'low'
        return 'none'

    def _calculate_darkbert_illegal_confidence(self, darkbert_scores: Dict[str, float]) -> float:
        if not darkbert_scores:
            return 0.0
        illegal_cats = ['violence', 'drugs', 'arms', 'financial', 'hacking', 'porn']
        scores = [darkbert_scores.get(c, 0.0) for c in illegal_cats]
        return float(min(max(scores), 1.0))

    def _calculate_severity(self, category: str, match_count: int, text_length: int) -> str:
        match_ratio = match_count / max(text_length, 1)
        high_cats = ['violence', 'drugs', 'arms', 'porn']
        medium_cats = ['gambling', 'electronic', 'crypto', 'financial', 'hacking']

        if category in high_cats:
            base = 'high'
        elif category in medium_cats:
            base = 'medium'
        else:
            base = 'low'

        if match_count >= 10 or match_ratio > 0.05:
            return 'high'
        elif match_count >= 3 or match_ratio > 0.01:
            return base
        else:
            return 'medium' if base == 'high' else 'low'

    def _calculate_illegal_confidence(self, total_matches: int, text_length: int,
                                      category_count: int) -> float:
        if total_matches == 0:
            return 0.0
        match_ratio = min(total_matches / max(text_length, 1), 1.0)
        ratio_score = match_ratio * 50
        category_score = min(category_count * 15, 30)
        return min((ratio_score + category_score) / 100.0, 1.0)

    def _default_keywords(self) -> Dict:
        return {
            'violence': [
                'kill', 'murder', 'violence', 'violent', 'attack', 'shooting',
                'terrorist', 'terrorism', 'assault', 'stab', 'shoot',
                'massacre', 'homicide', 'assassination', 'execute', 'execution',
                'kidnap', 'abduction', 'hostage', 'ransom', 'torture',
                '살인', '폭력', '테러', '총격', '위협', '공격', '납치'
            ],
            'gambling': [
                'gambling', 'gamble', 'casino', 'poker', 'betting', 'bet',
                'slot', 'roulette', 'lottery', 'jackpot', 'wager',
                'blackjack', 'craps', 'baccarat', 'odds', 'payout',
                '도박', '카지노', '포커', '베팅', '슬롯', '복권', '잭팟'
            ],
            'electronic': [
                'malware', 'virus', 'botnet', 'trojan', 'spyware',
                'ddos', 'ransomware', 'adware', 'keylogger', 'rootkit', 'worm',
                'backdoor', 'payload', 'shellcode', 'infostealer', 'cryptolocker',
                'zero-day', 'buffer overflow', 'privilege escalation',
                '악성코드', '바이러스', '해킹툴', '스파이웨어', '트로이목마'
            ],
            'drugs': [
                'cocaine', 'heroin', 'methamphetamine', 'fentanyl', 'mdma',
                'lsd', 'cannabis', 'marijuana', 'drug', 'narcotic',
                'opium', 'morphine', 'oxycodone', 'xanax', 'ghb',
                'psilocybin', 'ecstasy', 'amphetamine', 'crystal meth', 'ketamine',
                '약물', '마약', '코카인', '헤로인', '대마', '엑스터시'
            ],
            'crypto': [
                'bitcoin', 'cryptocurrency', 'crypto', 'blockchain', 'wallet',
                'ethereum', 'btc', 'eth', 'altcoin', 'mining', 'exchange',
                'monero', 'zcash', 'litecoin', 'token', 'coin',
                'private key', 'seed phrase', 'smart contract', 'defi', 'nft',
                '비트코인', '암호화폐', '블록체인', '지갑', '마이닝'
            ],
            'arms': [
                'firearms', 'gun', 'rifle', 'pistol', 'ammunition', 'explosives',
                'bomb', 'grenade', 'weapon', 'arms', 'sniper',
                'assault rifle', 'shotgun', 'revolver',
                'ak-47', 'glock', 'silencer', 'suppressor',
                'explosive', 'dynamite', 'c4', 'detonator',
                '총', '화기', '폭탄', '수류탄', '무기', '소총', '권총', '탄약'
            ],
            'financial': [
                'fraud', 'scam', 'fake', 'stolen', 'counterfeit', 'forgery',
                'phishing', 'money laundering', 'embezzlement', 'ponzi',
                'identity theft', 'credit card fraud', 'tax evasion',
                'pyramid scheme', 'money mule', 'structuring',
                '사기', '위조', '자금세탁', '폰지', '신용카드사기'
            ],
            'hacking': [
                'hack', 'hacking', 'carding', 'credit card', 'cvv',
                'dump', 'fullz', 'ssn', 'dox', 'breached', 'leaked',
                'cracking', 'brute force', 'sql injection', 'webshell',
                'stolen credentials', 'personal data',
                '해킹', '카딩', '개인정보', '데이터탈취'
            ],
            'porn': [
                'cp', 'child', 'minor', 'underage', 'teen',
                'adult', 'porn', 'xxx', 'sex', 'escort', 'prostitution',
                'abuse', 'nude', 'lolita', 'pedophile', 'pedophilia',
                'incest', 'rape', 'trafficking',
                '아동', '음란', '성인', '성적학대', '미성년자'
            ],
            'others': [
                'illegal', 'unlawful', 'banned', 'prohibited', 'restricted',
                'crime', 'criminal', 'felony', 'offense', 'offender',
                'jail', 'prison', 'arrest', 'dea', 'fbi', 'interpol',
                '불법', '금지', '범죄', '범죄자', '감옥', '체포'
            ]
        }
