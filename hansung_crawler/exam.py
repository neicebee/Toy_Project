import json

link_data = {
    "home": 'https://www.hansung.ac.kr/bbs/hansung/143/artclList.do',         # 메인 홈페이지
    "ce": 'https://hansung.ac.kr/bbs/CSE/1248/artclList.do',                 # 컴퓨터공학부
    "elc": 'https://www.hansung.ac.kr/bbs/HmnArt/53/artclList.do',            # 영미문학문화트랙
    "eli": 'https://www.hansung.ac.kr/bbs/HmnArt/67/artclList.do',            # 영미언어정보트랙
    "ke": 'https://www.hansung.ac.kr/bbs/HmnArt/55/artclList.do',             # 한국어교육트랙
    "lcc": 'https://www.hansung.ac.kr/bbs/HmnArt/56/artclList.do',            # 문학문화콘텐츠트랙
    "gh": 'https://www.hansung.ac.kr/bbs/HmnArt/57/artclList.do',             # 글로컬역사트랙
    "hcc": 'https://www.hansung.ac.kr/bbs/HmnArt/58/artclList.do',            # 역사문화콘텐츠트랙
    "kic": 'https://www.hansung.ac.kr/bbs/HmnArt/59/artclList.do',            # 지식정보문화트랙
    "dhi": 'https://www.hansung.ac.kr/bbs/HmnArt/60/artclList.do',            # 디지털인문정보학트랙
    "hcq": 'https://www.hansung.ac.kr/bbs/HmnArt/57/artclList.do',            # 역사문화큐레이션트랙
    "hc": 'https://www.hansung.ac.kr/bbs/HmnArt/58/artclList.do',             # 역사콘텐츠트랙
    "ecc": 'https://www.hansung.ac.kr/bbs/HmnArt/1088/artclList.do',          # 영미문화콘텐츠트랙
    "gt": 'https://www.hansung.ac.kr/bbs/SclScn/82/artclList.do',             # 국제무역트랙
    "gb": 'https://www.hansung.ac.kr/bbs/SclScn/80/artclList.do',             # 글로벌비즈니스트랙
    "fda": 'https://www.hansung.ac.kr/bbs/SclScn/70/artclList.do',            # 금융,데이터분석트랙
    "pa": 'https://www.hansung.ac.kr/bbs/SclScn/71/artclList.do',             # 공공행정트랙
    "lp": 'https://www.hansung.ac.kr/bbs/SclScn/72/artclList.do',             # 법과정책트랙
    "re": 'https://www.hansung.ac.kr/bbs/SclScn/73/artclList.do',             # 부동산트랙
    "stp": 'https://www.hansung.ac.kr/bbs/SclScn/74/artclList.do',            # 스마트도시,교통계획트랙
    "bdm": 'https://www.hansung.ac.kr/bbs/Design/93/artclList.do',            # 뷰티디자인매니지먼트학과
    "ele": 'https://www.hansung.ac.kr/bbs/Engineering/102/artclList.do',       # 전자트랙
    "ss": 'https://www.hansung.ac.kr/bbs/Engineering/984/artclList.do',       # 시스템반도체트랙
    "esd": 'https://www.hansung.ac.kr/bbs/Engineering/1365/artclList.do',     # 기계시스템디자인트랙
    "arc": 'https://www.hansung.ac.kr/bbs/Engineering/1366/artclList.do',     # ai로봇융합트랙
    "ins": 'https://www.hansung.ac.kr/bbs/Engineering/103/artclList.do',       # 정보시스템트랙
    "ed": 'https://www.hansung.ac.kr/bbs/Engineering/104/artclList.do',        # 기계설계트랙
    "ea": 'https://www.hansung.ac.kr/bbs/Engineering/105/artclList.do',        # 기계자동화트랙
    "intels": 'https://www.hansung.ac.kr/bbs/Engineering/106/artclList.do',    # 지능시스템트랙
    "iot": 'https://www.hansung.ac.kr/bbs/Engineering/107/artclList.do',       # 사물인터넷트랙
    "cs": 'https://www.hansung.ac.kr/bbs/Engineering/108/artclList.do',        # 사이버보안트랙
    "ice": 'https://www.hansung.ac.kr/bbs/Engineering/109/artclList.do',       # ict융합엔터테인먼트트랙
    "ie": 'https://www.hansung.ac.kr/bbs/Engineering/806/artclList.do',        # 산업공학트랙
    "smic": 'https://www.hansung.ac.kr/bbs/Engineering/996/artclList.do',     # 스마트제조혁신컨설팅학과
    "tt": 'https://www.hansung.ac.kr/bbs/CreCon/863/artclList.do',             # 상상력인재학부
    "t_lcc": 'https://www.hansung.ac.kr/bbs/CreCon/993/artclList.do',          # 문학문화콘텐츠학과
    "aa": 'https://www.hansung.ac.kr/bbs/CreCon/987/artclList.do',             # ai응용학과
    "asec": 'https://www.hansung.ac.kr/bbs/CreCon/1076/artclList.do',          # 융합보안학과
    "fm": 'https://www.hansung.ac.kr/bbs/CreCon/1142/artclList.do'             # 미래모빌리티학과
}

file_name = "links/hansung_links.json"
with open(file_name, 'w', encoding='utf-8') as f:
    json.dump(link_data, f, ensure_ascii=False, indent=4)
print(f"\n데이터가 '{file_name}' 파일에 저장되었습니다.")