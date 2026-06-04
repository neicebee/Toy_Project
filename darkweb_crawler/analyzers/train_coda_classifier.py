"""
CoDA 분류기 학습 스크립트 (한 번만 실행)
DarkBERT 임베딩 + LogisticRegression

실행: python3 analyzers/train_coda_classifier.py
"""

import os
import logging
import numpy as np
import pandas as pd
from pathlib import Path

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s')
logger = logging.getLogger(__name__)

DATA_PATH   = str(Path(__file__).parent.parent / 'data' / 'processed_coda_data_final.csv')
MODEL_OUT   = str(Path(__file__).parent.parent / 'data' / 'coda_classifier.pkl')
EMBED_CACHE = str(Path(__file__).parent.parent / 'data' / 'coda_embeddings.npz')
EXCLUDE_CATEGORIES = ['Others']
DARKBERT_MODEL = 's2w-ai/darkbert'
BATCH_SIZE  = 16
MAX_LEN     = 512


def get_embeddings(texts, tokenizer, model, device, batch_size=BATCH_SIZE):
    import torch
    all_vecs = []
    total = len(texts)
    for i in range(0, total, batch_size):
        batch = texts[i:i + batch_size]
        encoded = tokenizer(batch, truncation=True, max_length=MAX_LEN,
                            padding=True, return_tensors='pt')
        encoded = {k: v.to(device) for k, v in encoded.items()}
        with torch.no_grad():
            outputs = model(**encoded)
            mask = encoded['attention_mask'].unsqueeze(-1).float()
            vecs = (outputs.last_hidden_state * mask).sum(dim=1) / mask.sum(dim=1)
            all_vecs.append(vecs.cpu().numpy())
        if (i // batch_size + 1) % 10 == 0:
            logger.info(f"  임베딩 진행: {min(i+batch_size, total)}/{total}")
    return np.vstack(all_vecs)


def main():
    import torch
    import transformers
    from sklearn.linear_model import LogisticRegression
    from sklearn.preprocessing import LabelEncoder
    from sklearn.model_selection import train_test_split
    from sklearn.metrics import classification_report
    import joblib

    os.makedirs(os.path.dirname(MODEL_OUT), exist_ok=True)

    # 1. 전체 데이터 로드
    logger.info("📂 데이터 로드 중...")
    full_df = pd.read_csv(DATA_PATH)
    full_df['텍스트'] = full_df['텍스트'].astype(str)
    full_df['카테고리'] = full_df['카테고리'].astype(str)
    logger.info(f"  전체: {len(full_df)}개")

    # 2. 제외 카테고리 필터
    df = full_df[~full_df['카테고리'].isin(EXCLUDE_CATEGORIES)].reset_index()
    original_indices = df['index'].values
    texts = df['텍스트'].tolist()
    labels = df['카테고리'].tolist()
    logger.info(f"  필터 후: {len(df)}개, 카테고리: {sorted(set(labels))}")

    # 3. 레이블 인코딩
    le = LabelEncoder()
    y = le.fit_transform(labels)

    # 4. 임베딩 캐시 확인
    if os.path.exists(EMBED_CACHE):
        logger.info(f"⚡ 캐시된 임베딩 로드: {EMBED_CACHE}")
        X_all = np.load(EMBED_CACHE)['X']
        X = X_all[original_indices]
        logger.info(f"  임베딩 shape: {X.shape}")
    else:
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
        model = model.to(device)
        logger.info(f"  device: {device}")

        logger.info("🧮 전체 데이터 임베딩 추출 중...")
        X_all = get_embeddings(full_df['텍스트'].tolist(), tokenizer, model, device)
        np.savez(EMBED_CACHE, X=X_all)
        logger.info(f"  임베딩 캐시 저장: {EMBED_CACHE}")
        X = X_all[original_indices]

    # 5. 학습/검증 분할
    X_train, X_val, y_train, y_val = train_test_split(
        X, y, test_size=0.15, random_state=42, stratify=y
    )

    # 6. LogisticRegression 학습
    logger.info("🏋️ 분류기 학습 중...")
    clf = LogisticRegression(max_iter=1000, class_weight='balanced', C=1.0, solver='lbfgs')
    clf.fit(X_train, y_train)

    # 7. 평가
    y_pred = clf.predict(X_val)
    logger.info("\n📊 검증 결과:")
    print(classification_report(y_val, y_pred, target_names=le.classes_))

    # 8. 저장
    joblib.dump({'classifier': clf, 'label_encoder': le}, MODEL_OUT)
    logger.info(f"✅ 모델 저장 완료: {MODEL_OUT}")


if __name__ == '__main__':
    main()
