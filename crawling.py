import os
import time
import win32pipe, win32file
import json
import requests
from bs4 import BeautifulSoup

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
    
    response = win32pipe.ReadFile(pipe,256)
    if response == "경제" or response == "정치" or response == "생활/문화" or response == "IT/과학" or response == "세계" or response == "사회":
        page_client = 1   
        display_count = 15
        news_title_link = get_news_list(response, display_count, page_client)
    
    
    
    
    win32file.CloseHandle(pipe)
    


def get_news_list(category_keyword, display_count, page_client):
    url = "https://openapi.naver.com/v1/search/news.json"
    headers = {
        "X-Naver-Client-Id": "jMSpF8hz4_ZCjwdrdRv2",
        "X-Naver-Client-Secret": "3UsFl82dKQ"
    }
    
    params = {
        "query": category_keyword,  # 예: "경제 뉴스"
        "display": display_count,   # 가져올 기사 수 (최대 100)
        "start": 1*page_client,                 # 시작 인덱스
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