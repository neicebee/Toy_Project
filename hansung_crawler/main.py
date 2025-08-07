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
    
    # 공지사항 크롤링 객체 생성
    Crawler = get_html('https://www.hansung.ac.kr')
    # num 타입별 분류 후 저장할 최종 딕셔너리
    final_categorized_data = {
        'categories': {},  # num이 문자열
        'notices_by_id': {} # num이 숫자
    }
    
    file_name = 'links/hansung_links.json'
    all_links = {}
    if os.path.exists(file_name):
        try:
            with open(file_name, 'r', encoding='utf-8') as f:
                all_links = json.load(f)
            print(f"'{file_name}' 파일에서 링크 정보를 성공적으로 로드했습니다.")
        except json.JSONDecodeError:
            print(f"'{file_name}' 파일이 유효한 JSON 형식이 아닙니다.")
            exit() # 오류 발생 시 프로그램 종료
        except Exception as e:
            print(f"'{file_name}' 파일 로드 중 오류 발생: {e}")
            exit()
    else:
        print(f"'{file_name}' 파일을 찾을 수 없습니다. 프로그램을 종료합니다.")
        exit()
    
    for link_name, url in all_links.items():
        Crawler.change_url(url)
        get_notices(link_name, Crawler, layout, final_categorized_data)
        dict_initialization(final_categorized_data)