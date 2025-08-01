import requests
from typing import Union

class get_html:
    def __init__(self, url: str) -> None:
        if not url:
            self.url = None
            return
        self.url = url
        self.hdr = {'Accept-Language': 'ko_KR,en;q=0.8', \
            'User-Agent': ('Mozilla/5.0 (Linux; Android 6.0; \
                Nexus 5 Build/MRA58N) AppleWebKit/537.36 \
                    (KHTML, like Gecko) Chrome/78.0.3904.70 Mobile \
                        Safari/537.36')}
    
    def get_req(self) -> Union[str, int]:
        if not self.url:
            return None
        try:
            with requests.Session() as s:
                req = s.get(self.url, headers=self.hdr, timeout=10)
                return req.status_code if not req.status_code == 200 else req.text
        except requests.exceptions.RequestException as e:
            print(f"네트워크 요청 중 예외 발생: {e}")
            return -1
        
    def post_req(self, post_data) -> Union[str, int]:
        if not self.url:
            return None
        try:
            with requests.Session() as s:
                req = s.post(self.url, headers=self.hdr, data=post_data, timeout=10)
                return req.status_code if not req.status_code == 200 else req.text
        except requests.exceptions.RequestException as e:
            print(f"네트워크 요청 중 예외 발생: {e}")
            return -1
    
    def get_content(self) -> Union[bytes, int]:
        if not self.url:
            return None
        with requests.Session() as s:
            req = s.get(self.url, headers=self.hdr)
            output = req.status_code if not req.status_code == 200 else req.content
        return output
        
    def check_url(self) -> str:
        return self.url
    
    def change_url(self, url: str) -> None:
        if not url:
            return
        self.url = url