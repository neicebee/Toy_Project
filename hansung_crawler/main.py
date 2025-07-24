from get_html import get_html
from make_json import parse_table_data
from bs4 import BeautifulSoup
import re, json

callback_url = 'https://www.hansung.ac.kr/bbs/hansung/143/artclList.do'

home_notices_layout = '5Op5N6HXicqz2jqh4sxgz7ECT9MbeWxhVFwsIyGN%2F0c%3D'

# ce_notices_url = 'https://hansung.ac.kr/CSE/10766/subview.do?enc=Zm5jdDF8QEB8JTJGYmJzJTJGQ1NFJTJGMTI0OCUyRmFydGNsTGlzdC5kbyUzRg%3D%3D'

# 공지사항 크롤링 객체 생성
Crawler = get_html(callback_url)

# num 타입별 분류 후 저장할 최종 딕셔너리
categorized_data = {
    'categories': {},  # num이 문자열
    'notices_by_id': {} # num이 숫자
}

# 페이지 5부터 1까지 공지사항 크롤링
for page in range(1, 6):
    # 메인 공지사항 post 데이터 생성
    home_notices_data = {
        'layout': home_notices_layout,
        'page': page,
        'isViewMine': False,
        'srchColumn': 'sj'
    }
    data = Crawler.post_req(home_notices_data)
    soup = BeautifulSoup(data, 'html.parser') if isinstance(data, str) else print(data)
    n_table = soup.select_one('table.board-table.horizon1')
    parsed_records = parse_table_data(n_table)

    for record in parsed_records:
        if 'num' in record:
            num_value = record['num']
            try:
                num_as_int = int(num_value)
                item_content = record.copy()
                del item_content['num']
                categorized_data['notices_by_id'][num_value] = item_content # 원본 num_value를 키로 사용
            except ValueError:
                if num_value not in categorized_data['categories']:
                    categorized_data['categories'][num_value] = []
                item_content = record.copy()
                del item_content['num']
                categorized_data['categories'][num_value].append(item_content)
    print(f"{page} 페이지 파싱 완료...")
    
# JSON 파일로 저장
output_file_name = "categorized_notices.json"
try:
    with open(output_file_name, 'w', encoding='utf-8') as f:
        json.dump(categorized_data, f, ensure_ascii=False, indent=4)
    print(f"모든 데이터가 '{output_file_name}' 파일에 성공적으로 저장되었습니다.")
except IOError as e:
    print(f"파일 저장 중 오류 발생: {e}")