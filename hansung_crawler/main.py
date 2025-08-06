from get_html import get_html
from utils import parse_table_data, make_categorized_data
from bs4 import BeautifulSoup
import os, json

def get_notices(text: str, Crawler, layout, final_categorized_data):
    output_file_name = f"notices_info/{text}_categorized_notices.json"
    SHOULD_RUN_FULL_CRAWL = True # 전체 크롤링 여부 결정 플래그

    if os.path.exists(output_file_name):
        print(f"'{output_file_name}' 파일 존재\n최신 내용 확인 중...")
        # 기존 파일 내의 데이터 가져오기
        try:
            with open(output_file_name, 'r', encoding='utf-8') as f:
                existing_data = json.load(f) # 기존 JSON 데이터 로드
        except json.JSONDecodeError:
            print("Error: 전체 크롤링 진행...")
            SHOULD_RUN_FULL_CRAWL = True
        except Exception as e:
            print(f"{e}: 전체 크롤링 진행...")
            SHOULD_RUN_FULL_CRAWL = True
        # 데이터 변경 검증을 위한 임시 크롤링 데이터
        temp_page1_data_post_payload = { # post payload
            'layout': layout,
            'page': 1,
            'isViewMine': False,
            'srchColumn': 'sj'
        }
        current_page1_raw_html = Crawler.post_req(temp_page1_data_post_payload)
        if not isinstance(current_page1_raw_html, str):
            print("1페이지 크롤링 실패. 전체 크롤링 진행...")
            SHOULD_RUN_FULL_CRAWL = True
        else:
            soup_page1 = BeautifulSoup(current_page1_raw_html, 'html.parser')
            n_table_page1 = soup_page1.select_one('table.board-table.horizon1')
            if not n_table_page1:
                print("1페이지 테이블 색인 불가. 전체 크롤링 진행...")
                SHOULD_RUN_FULL_CRAWL = True
            else:
                # parse_table_data를 통해 레코드 리스트를 얻고, 이를 categorize_data 형식으로 변환
                temp_categorized_data = {
                    'categories': {},  # num이 문자열
                    'notices_by_id': {} # num이 숫자
                }
                make_categorized_data(parse_table_data(n_table_page1), categorized_data=temp_categorized_data)
                is_same_content = True
                # 일반공지 및 전체게시판공지 비교
                major_categories_to_compare = ['일반공지', '전체게시판공지']
                num_records_to_compare_per_category = 5
                for category_name in major_categories_to_compare:
                    existing_list = existing_data.get('categories', {}).get(category_name, [])
                    current_list = temp_categorized_data.get('categories', {}).get(category_name, [])
                    # 두 리스트의 시작 부분만 비교
                    if existing_list[:num_records_to_compare_per_category]!=current_list[:num_records_to_compare_per_category]:
                        print(f"'{category_name}' 카테고리 새로운 내용 감지...")
                        is_same_content = False
                        break
                if is_same_content:
                    existing_notices_by_id = existing_data.get('notices_by_id', {})
                    current_notices_by_id = temp_categorized_data.get('notices_by_id', {})
                    # 기존 파일과 현재 크롤링한 1페이지 notices_by_id가 비어있지 않은지 확인
                    if not existing_notices_by_id:
                        print("전체 크롤링 진행...")
                        SHOULD_RUN_FULL_CRAWL = True
                        is_same_content = False # 비어있으면 다름
                    elif not current_notices_by_id:
                        print("전체 크롤링 진행...")
                        SHOULD_RUN_FULL_CRAWL = True
                        is_same_content = False # 비어있으면 다름
                    else:
                        first_existing_id_key = next(iter(existing_notices_by_id))
                        first_current_id_key = next(iter(current_notices_by_id))
                        print(f"기존 첫 번째 ID: {first_existing_id_key}\n현재 첫 번째 ID: {first_current_id_key}")
                        # 첫 번째 ID가 서로 다르면 내용이 변경된 것으로 판단
                        if first_existing_id_key != first_current_id_key:
                            print("변경 감지...")
                            is_same_content = False
                            SHOULD_RUN_FULL_CRAWL = True
                        else:
                            # 첫 번째 ID가 같으면 그 해당 항목의 내용을 비교
                            first_existing_item = existing_notices_by_id.get(first_existing_id_key)
                            first_current_item = current_notices_by_id.get(first_current_id_key)
                            if first_existing_item == first_current_item:
                                SHOULD_RUN_FULL_CRAWL = False # 모든 비교 통과 -> 전체 크롤링 건너뛰기
                            else:
                                print("내용 변경 감지...")
                                is_same_content = False
                                SHOULD_RUN_FULL_CRAWL = True
                else:
                    print("전체 크롤링 진행...")
                    SHOULD_RUN_FULL_CRAWL = True
        if SHOULD_RUN_FULL_CRAWL:
            print(f"\n--- {text}: 전체 크롤링 시작 ---")
            # 페이지 1부터 10까지 공지사항 크롤링 (페이지 10까지 확인한다고 가정)
            for page in range(1, 11):
                home_notices_data = {
                    'layout': layout,
                    'page': page,
                    'isViewMine': False,
                    'srchColumn': 'sj'
                }
                data = Crawler.post_req(home_notices_data)
                soup = BeautifulSoup(data, 'html.parser') if isinstance(data, str) else print(data)
                n_table = soup.select_one('table.board-table.horizon1')
                make_categorized_data(parse_table_data(n_table), categorized_data=final_categorized_data)
                print(f"{page} 페이지 파싱 완료...")
            try:
                with open(output_file_name, 'w', encoding='utf-8') as f:
                    json.dump(final_categorized_data, f, ensure_ascii=False, indent=4)
                print(f"모든 데이터가 '{output_file_name}' 파일에 성공적으로 저장되었습니다.")
            except IOError as e:
                print(f"파일 저장 중 오류 발생: {e}")
        else:
            print("기존 파일과 내용이 같아 크롤링 및 파일 저장 로직을 실행하지 않았습니다.")
    else:
        print(f"{text}: 전체 크롤링 진행...")
        # 페이지 1부터 11까지 공지사항 크롤링
        for page in range(1, 11):
            # 메인 공지사항 post 데이터 생성
            home_notices_data = {
                'layout': layout,
                'page': page,
                'isViewMine': False,
                'srchColumn': 'sj'
            }
            data = Crawler.post_req(home_notices_data)
            soup = BeautifulSoup(data, 'html.parser') if isinstance(data, str) else print(data)
            n_table = soup.select_one('table.board-table.horizon1')
            if '게시물이(가) 없습니다.' in n_table.text:
                print(f"{page} 페이지부터 파싱된 공지사항이 없습니다. 크롤링을 중단합니다.")
                break
            make_categorized_data(parse_table_data(n_table), categorized_data=final_categorized_data)
            print(f"{page} 페이지 파싱 완료...")
        try:
            with open(output_file_name, 'w', encoding='utf-8') as f:
                json.dump(final_categorized_data, f, ensure_ascii=False, indent=4)
            print(f"모든 데이터가 '{output_file_name}' 파일에 성공적으로 저장되었습니다.")
        except IOError as e:
            print(f"파일 저장 중 오류 발생: {e}")
            
def dict_initialization(final_categorized_data):
    final_categorized_data['categories'] = {}
    final_categorized_data['notices_by_id'] = {}
    
if __name__=="__main__":
    layout = ''
    # 메인 홈페이지
    home = 'https://www.hansung.ac.kr/bbs/hansung/143/artclList.do'
    # 컴퓨터공학부
    ce = 'https://hansung.ac.kr/bbs/CSE/1248/artclList.do'
    # 영미문학문화트랙
    elc = 'https://www.hansung.ac.kr/bbs/HmnArt/53/artclList.do'
    # 영미언어정보트랙
    eli = 'https://www.hansung.ac.kr/bbs/HmnArt/67/artclList.do'
    # 한국어교육트랙
    ke = 'https://www.hansung.ac.kr/bbs/HmnArt/55/artclList.do'
    # 문학문화콘텐츠트랙
    lcc = 'https://www.hansung.ac.kr/bbs/HmnArt/56/artclList.do'
    # 글로컬역사트랙
    gh = 'https://www.hansung.ac.kr/bbs/HmnArt/57/artclList.do'
    # 역사문화콘텐츠트랙
    hcc = 'https://www.hansung.ac.kr/bbs/HmnArt/58/artclList.do'
    # 지식정보문화트랙
    kic = 'https://www.hansung.ac.kr/bbs/HmnArt/59/artclList.do'
    # 디지털인문정보학트랙
    dhi = 'https://www.hansung.ac.kr/bbs/HmnArt/60/artclList.do'
    # 역사문화큐레이션트랙
    hcq = 'https://www.hansung.ac.kr/bbs/HmnArt/57/artclList.do'
    # 역사콘텐츠트랙
    hc = 'https://www.hansung.ac.kr/bbs/HmnArt/58/artclList.do'
    # 영미문화콘텐츠트랙
    ecc = 'https://www.hansung.ac.kr/bbs/HmnArt/1088/artclList.do'
    # 국제무역트랙
    gt = 'https://www.hansung.ac.kr/bbs/SclScn/82/artclList.do'
    # 글로벌비즈니스트랙
    gb = 'https://www.hansung.ac.kr/bbs/SclScn/80/artclList.do'
    # 금융,데이터분석트랙
    fda = 'https://www.hansung.ac.kr/bbs/SclScn/70/artclList.do'
    # 공공행정트랙
    pa = 'https://www.hansung.ac.kr/bbs/SclScn/71/artclList.do'
    # 법과정책트랙
    lp = 'https://www.hansung.ac.kr/bbs/SclScn/72/artclList.do'
    # 부동산트랙
    re = 'https://www.hansung.ac.kr/bbs/SclScn/73/artclList.do'
    # 스마트도시,교통계획트랙
    stp = 'https://www.hansung.ac.kr/bbs/SclScn/74/artclList.do'
    # 뷰티디자인매니지먼트학과
    bdm = 'https://www.hansung.ac.kr/bbs/Design/93/artclList.do'
    # 전자트랙
    ele = 'https://www.hansung.ac.kr/bbs/Engineering/102/artclList.do'
    # 시스템반도체트랙
    ss = 'https://www.hansung.ac.kr/bbs/Engineering/984/artclList.do'
    # 기계시스템디자인트랙
    esd = 'https://www.hansung.ac.kr/bbs/Engineering/1365/artclList.do'
    # ai로봇융합트랙
    arc = 'https://www.hansung.ac.kr/bbs/Engineering/1366/artclList.do'
    # 정보시스템트랙
    ins = 'https://www.hansung.ac.kr/bbs/Engineering/103/artclList.do'
    # 기계설계트랙
    ed = 'https://www.hansung.ac.kr/bbs/Engineering/104/artclList.do'
    # 기계자동화트랙
    ea = 'https://www.hansung.ac.kr/bbs/Engineering/105/artclList.do'
    # 지능시스템트랙
    intels = 'https://www.hansung.ac.kr/bbs/Engineering/106/artclList.do'
    # 사물인터넷트랙
    iot = 'https://www.hansung.ac.kr/bbs/Engineering/107/artclList.do'
    # 사이버보안트랙
    cs = 'https://www.hansung.ac.kr/bbs/Engineering/108/artclList.do'
    # ict융합엔터테인먼트트랙
    ice = 'https://www.hansung.ac.kr/bbs/Engineering/109/artclList.do'
    # 산업공학트랙
    ie = 'https://www.hansung.ac.kr/bbs/Engineering/806/artclList.do'
    # 스마트제조혁신컨설팅학과
    smic = 'https://www.hansung.ac.kr/bbs/Engineering/996/artclList.do'
    # 상상력인재학부
    tt = 'https://www.hansung.ac.kr/bbs/CreCon/863/artclList.do'
    # 문학문화콘텐츠학과
    t_lcc = 'https://www.hansung.ac.kr/bbs/CreCon/993/artclList.do'
    # ai응용학과
    aa = 'https://www.hansung.ac.kr/bbs/CreCon/987/artclList.do'
    # 융합보안학과
    asec = 'https://www.hansung.ac.kr/bbs/CreCon/1076/artclList.do'
    # 미래모빌리티학과
    fm = 'https://www.hansung.ac.kr/bbs/CreCon/1142/artclList.do'
    

    # 공지사항 크롤링 객체 생성
    Crawler = get_html(home)

    # num 타입별 분류 후 저장할 최종 딕셔너리
    final_categorized_data = {
        'categories': {},  # num이 문자열
        'notices_by_id': {} # num이 숫자
    }
    
    get_notices('home', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(ce)
    get_notices('ce', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(elc)
    get_notices('elc', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(eli)
    get_notices('eli', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(ke)
    get_notices('ke', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(lcc)
    get_notices('lcc', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(gh)
    get_notices('gh', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(hcc)
    get_notices('hcc', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(kic)
    get_notices('kic', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(dhi)
    get_notices('dhi', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(hcq)
    get_notices('hcq', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(hc)
    get_notices('hc', Crawler, layout, final_categorized_data)
    
    dict_initialization(final_categorized_data)
    Crawler.change_url(ecc)
    get_notices('ecc', Crawler, layout, final_categorized_data)