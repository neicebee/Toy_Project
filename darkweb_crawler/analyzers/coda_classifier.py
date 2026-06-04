"""
CoDA 분류기 추론 모듈
학습된 DarkBERT + LogisticRegression 모델로 범죄 카테고리 분류
"""

import logging
import importlib
import os
from pathlib import Path
from typing import Dict, Optional

import numpy as np

from utils.html_cleaner import HTMLCleaner

logger = logging.getLogger(__name__)

MODEL_PATH   = str(Path(__file__).parent.parent / 'data' / 'coda_classifier.pkl')
DARKBERT_MODEL = 's2w-ai/darkbert'
MAX_LEN      = 512
# 점수 차이가 이 값보다 작으면 "분류 불확실" 처리
CONFIDENCE_GAP_THRESHOLD = 0.10


class CoDAClassifier:
    """학습된 DarkBERT + LogisticRegression 기반 CoDA 분류기"""

    def __init__(self, model_path: str = MODEL_PATH):
        self.model_path = model_path
        self._clf = None
        self._le  = None
        self._tokenizer = None
        self._model = None
        self._device = None
        self._loaded = False

    def _load(self):
        if self._loaded:
            return

        self._loaded = True

        if not os.path.exists(self.model_path):
            logger.warning(f"⚠️ 학습된 모델 없음: {self.model_path}")
            logger.warning("   python3 analyzers/train_coda_classifier.py 를 먼저 실행하세요")
            return

        try:
            joblib = importlib.import_module('joblib')
            saved = joblib.load(self.model_path)
            self._clf = saved['classifier']
            self._le  = saved['label_encoder']
            logger.info(f"✅ CoDA 분류기 로드: {list(self._le.classes_)}")
        except Exception as e:
            logger.error(f"❌ 분류기 로드 실패: {e}")
            return

        try:
            torch = importlib.import_module('torch')
            transformers = importlib.import_module('transformers')

            logger.info("🔄 DarkBERT 로드 중...")
            tokenizer = transformers.AutoTokenizer.from_pretrained(DARKBERT_MODEL)
            model = transformers.AutoModel.from_pretrained(DARKBERT_MODEL)

            special_tokens = {'additional_special_tokens': ['[NUM]', '[TIME]', '[CRYPTO]', '[URL]', '[TOKEN]']}
            tokenizer.add_special_tokens(special_tokens)
            model.resize_token_embeddings(len(tokenizer))
            model.eval()

            if torch.backends.mps.is_available():
                device = torch.device('mps')
            elif torch.cuda.is_available():
                device = torch.device('cuda')
            else:
                device = torch.device('cpu')

            self._tokenizer = tokenizer
            self._model = model.to(device)
            self._device = device
            logger.info(f"✅ DarkBERT 로드 완료 (device={device})")
        except Exception as e:
            logger.error(f"❌ DarkBERT 로드 실패: {e}")

    def _embed(self, text: str) -> Optional[np.ndarray]:
        if self._tokenizer is None or self._model is None:
            return None

        try:
            torch = importlib.import_module('torch')
            encoded = self._tokenizer(
                text,
                truncation=True,
                max_length=MAX_LEN,
                padding=True,
                return_tensors='pt'
            )
            encoded = {k: v.to(self._device) for k, v in encoded.items()}

            with torch.no_grad():
                outputs = self._model(**encoded)
                mask = encoded['attention_mask'].unsqueeze(-1).float()
                vec = (outputs.last_hidden_state * mask).sum(dim=1) / mask.sum(dim=1)
                return vec.cpu().numpy()
        except Exception as e:
            logger.error(f"임베딩 오류: {e}")
            return None

    def classify(self, html: str) -> Dict:
        """
        HTML 콘텐츠를 분석하여 CoDA 범죄 카테고리 반환

        Returns:
            {
                'category': str,         # 최종 카테고리
                'confidence': float,     # 최고 확률
                'uncertain': bool,       # 1·2위 차이 < threshold
                'all_probs': dict,       # 카테고리별 확률
                'available': bool        # 모델 사용 가능 여부
            }
        """
        self._load()

        empty = {
            'category': 'unknown',
            'confidence': 0.0,
            'uncertain': True,
            'all_probs': {},
            'available': False
        }

        if self._clf is None or self._le is None:
            return empty

        text = HTMLCleaner.clean(html)[:8000]
        if not text:
            return empty

        vec = self._embed(text)
        if vec is None:
            return empty

        probs = self._clf.predict_proba(vec)[0]
        classes = self._le.classes_

        all_probs = {cls: round(float(p), 4) for cls, p in zip(classes, probs)}
        sorted_probs = sorted(all_probs.items(), key=lambda x: x[1], reverse=True)

        top1_cls, top1_prob = sorted_probs[0]
        top2_prob = sorted_probs[1][1] if len(sorted_probs) > 1 else 0.0
        gap = top1_prob - top2_prob
        uncertain = gap < CONFIDENCE_GAP_THRESHOLD

        if uncertain:
            logger.info(f"⚠️ CoDA 분류 불확실: {top1_cls} ({top1_prob:.2%}) vs {sorted_probs[1][0]} ({top2_prob:.2%}), 차이={gap:.2%}")
        else:
            logger.info(f"🎯 CoDA 분류: {top1_cls} ({top1_prob:.2%}), 2위 차이={gap:.2%}")

        return {
            'category': top1_cls,
            'confidence': round(top1_prob, 4),
            'uncertain': uncertain,
            'all_probs': dict(sorted_probs),
            'available': True
        }

    @property
    def is_ready(self) -> bool:
        self._load()
        return self._clf is not None and self._model is not None
