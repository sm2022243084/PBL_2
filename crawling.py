import os
import time
import win32pipe, win32file

PIPE_NAME = r'\\.\pipe\NewsPipe'
client_id = "YOUR_CLIENT_ID"
client_secret = "YOUR_CLIENT_SECRET"

def main():
    pipe = win32pipe.CreateNamedPipe(
        PIPE_NAME,
        win32pipe.PIPE_ACCESS_DUPLEX,
        win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE | win32pipe.PIPE_WAIT,
        1, 65536, 65536,
        0,
        None
    )
    
    win32pipe.ConnectNamedPipe(pipe, None)
    print("pipe connected.")
    
    
    
    
    
    
    win32file.CloseHandle(pipe)
    


def get_news_list(category_keyword, display_count):
    url = "https://openapi.naver.com/v1/search/news.json"
    headers = {
        "X-Naver-Client-Id": client_id,
        "X-Naver-Client-Secret": client_secret
    }
    
    params = {
        "query": category_keyword,  # 예: "경제 뉴스"
        "display": display_count,   # 가져올 기사 수 (최대 100)
        "start": 1,                 # 시작 인덱스
        "sort": "date"              # 정렬 방식: date(최신순) 또는 sim(정확도순)
    }

    response = requests.get(url, headers=headers, params=params)
    
    if response.status_code == 200:
        items = response.json()['items']
        news_list = []
        for item in items:
            title = item['title'].replace("<b>", "").replace("</b>", "")  # 강조 제거
            originallink = item['originallink']
            news_list.append({
                "title": title,
                "link": originallink
            })
        return news_list
    else:
        print("API 호출 실패:", response.status_code)
        return []