import os
import time
import win32pipe, win32file
import json
import requests
from bs4 import BeautifulSoup

PIPE_NAME = r'\\.\pipe\NewsPipe'
client_id = "jMSpF8hz4_ZCjwdrdRv2"
client_secret = "3UsFl82dKQ"

CATEGORY_LIST = ["정치", "경제", "사회", "생활/문화", "IT/과학", "세계"]
last_category = "정치"  # 기본값 설정

def main():
    print("Waiting for pipe connection...")
    pipe = win32pipe.CreateNamedPipe(
        PIPE_NAME,
        win32pipe.PIPE_ACCESS_DUPLEX,
        win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE | win32pipe.PIPE_WAIT,
        1, 65536, 65536,
        0,
        None
    )

    win32pipe.ConnectNamedPipe(pipe, None)
    print("Pipe connected.")

    global last_category
    while True:
        try:
            result, _ = win32file.ReadFile(pipe, 2048)
            msg = result.decode().strip()
            print("Received:", msg)

            # 카테고리 처리
            if msg in CATEGORY_LIST:
                last_category = msg
                news_list = get_news_list(msg, display_count=15, page_client=1)
                res = '\n'.join(f"{item['title']}|{item['link']}" for item in news_list)
                win32file.WriteFile(pipe, res.encode())

            # 페이지 번호 처리
            elif msg.isdigit():
                page = int(msg)
                news_list = get_news_list(last_category, 15, page)
                res = '\n'.join(f"{item['title']}|{item['link']}" for item in news_list)
                win32file.WriteFile(pipe, res.encode())

            # 뉴스 링크 처리
            elif msg.startswith("http"):
                content = get_news_content(msg)
                win32file.WriteFile(pipe, content.encode('utf-8', errors='ignore'))

            else:
                win32file.WriteFile(pipe, b"요청을 이해할 수 없습니다.\n")

        except Exception as e:
            print("오류 발생:", e)
            break

    win32file.CloseHandle(pipe)

def get_news_list(category_keyword, display_count, page_client):
    url = "https://openapi.naver.com/v1/search/news.json"
    headers = {
        "X-Naver-Client-Id": client_id,
        "X-Naver-Client-Secret": client_secret
    }
    params = {
        "query": category_keyword,
        "display": display_count,
        "start": 1 + (page_client - 1) * display_count,
        "sort": "date"
    }

    try:
        response = requests.get(url, headers=headers, params=params)
        if response.status_code == 200:
            items = response.json().get('items', [])
            news_list = []
            for item in items:
                title = item['title'].replace("<b>", "").replace("</b>", "")
                originallink = item['originallink']
                news_list.append({
                    "title": title,
                    "link": originallink
                })
            return news_list
        else:
            print("뉴스 목록 요청 실패:", response.status_code)
            return []
    except Exception as e:
        print("뉴스 목록 예외:", e)
        return []

def get_news_content(url):
    try:
        response = requests.get(url, timeout=5)
        if response.status_code != 200:
            return "뉴스 본문 수집 실패 (코드 오류)"
        soup = BeautifulSoup(response.text, "html.parser")
        text = soup.get_text(separator=' ', strip=True)
        return text
    except Exception as e:
        return f"뉴스 본문 수집 실패 (예외: {e})"

if __name__ == "__main__":
    main()