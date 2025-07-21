from get_html import get_html
from bs4 import BeautifulSoup
import re

TAG = ['num', 'subject', 'write', 'date', 'access', 'file']

h_notices_url = 'https://www.hansung.ac.kr/hansung/8385/subview.do?enc=Zm5jdDF8QEB8JTJGYmJzJTJGaGFuc3VuZyUyRjE0MyUyRmFydGNsTGlzdC5kbyUzRmJic0NsU2VxJTNEJTI2YmJzT3BlbldyZFNlcSUzRCUyNmlzVmlld01pbmUlM0RmYWxzZSUyNnNyY2hDb2x1bW4lM0RzaiUyNnNyY2hXcmQlM0QlMjY%3D'
ce_notices_url = 'https://hansung.ac.kr/CSE/10766/subview.do?enc=Zm5jdDF8QEB8JTJGYmJzJTJGQ1NFJTJGMTI0OCUyRmFydGNsTGlzdC5kbyUzRg%3D%3D'

# 공지사항 크롤링 객체 생성
Crawler = get_html(h_notices_url)
data = Crawler.get_req()
soup = BeautifulSoup(data, 'html.parser') if isinstance(data, str) else print(data)
n_table = soup.select_one('table.board-table.horizon1')

if n_table:
    print("--- 추출 내용 ---")
    # thead 추출
    headers = [th.get_text(strip=True) for th in n_table.find('thead').find_all('th')]
    print("테이블 헤더:", headers)

    # table의 tr 추출
    data_rows = n_table.find('tbody').find_all('tr')
    
    # tr의 td 추출
    for row in data_rows:
        cells = row.find_all('td')
        for i, element in enumerate([cell.get_text(strip=True) for cell in cells]):
            if i==5 and element=='':
                element = '0'
            if i==1:
                temp = re.sub(r'\s+', ' ', element).strip()
                print(f"{TAG[i]}: {temp}")
            else:
                print(f"{TAG[i]}: {element}")
else:
    print("요소 검색 오류")

# with open('result.txt', 'wt') as f:
#     f.write(n_table.text)