import re, json

TAG = ['num', 'subject', 'write', 'date', 'access', 'file']

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
            for i, element in enumerate(elements):
                key = TAG[i]
                value = element
                if key == 'num':
                    if current_record: # 이전 레코드가 있으면 저장
                        parsed_records.append(current_record)
                    current_record = {} # 새 레코드 시작
                    current_record[key] = value
                elif current_record: # 현재 레코드에 데이터 추가
                    # 'access'와 'file'은 숫자로 변환 시도
                    if key in ['access', 'file']:
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