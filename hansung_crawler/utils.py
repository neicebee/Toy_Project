import re

TAG = ['num', 'subject', 'write', 'date', 'access', 'file']

# 공지사항 요소 파싱 메서드
def parse_table_data(n_table):
    # json 데이터 초기화
    parsed_records = []
    current_record = None
    if n_table:
        # table의 tr 추출
        data_rows = n_table.find('tbody').find_all('tr')
        # tr의 td 추출 및 딕셔너리 리스트로 변환
        for row in data_rows:
            cells = row.find_all('td')
            elements = [
                re.sub(r'\s+', ' ', cell.get_text(strip=True)).strip() if i==1 else cell.get_text(strip=True)
                for i, cell in enumerate(cells)
            ]
            for cell in cells:
                if 'td-subject' in cell.get('class', []):
                    a_tag = cell.find('a')
                    if a_tag and a_tag.get('href'):
                        extracted_href = a_tag.get('href')
                        break
            for i, element in enumerate(elements):
                key = TAG[i]
                value = element
                if key=='num':
                    current_record = {} # 새 레코드 시작
                    current_record[key] = value
                elif current_record: # 현재 레코드에 데이터 추가
                    if key=='access':
                        current_record['link'] = f"https://www.hansung.ac.kr{extracted_href}"
                    elif key=='file':
                        try:
                            current_record[key] = int(value)
                        except ValueError:
                            current_record[key] = 0 # 변환 실패 시 원본 문자열 유지
                    else:
                        current_record[key] = value
            if current_record: # 마지막 레코드 추가
                parsed_records.append(current_record)
    else:
        print("요소 검색 오류")   
    return parsed_records

# 파싱 데이터 딕셔너리 변환 메서드
def make_categorized_data(parsed_records, categorized_data):
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