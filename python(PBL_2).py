import os
import time
import win32pipe, win32file, win32api
import json
import requests
from bs4 import BeautifulSoup
import pywintypes

PIPE_NAME = r'\\.\pipe\NewsPipe'
client_id = "jMSpF8hz4_ZCjwdrdRv2"
client_secret = "3UsFl82dKQ"

def main():
    print("Creating named pipe...")
    
    # 파이프 생성
    pipe = win32pipe.CreateNamedPipe(
        PIPE_NAME,
        win32pipe.PIPE_ACCESS_DUPLEX,
        win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE | win32pipe.PIPE_WAIT,
        1, 65536, 65536,
        0,
        None
    )
    
    if pipe == win32file.INVALID_HANDLE_VALUE:
        print("Failed to create pipe")
        return
    
    print("Waiting for pipe connection...")
    
    try:
        # 클라이언트 연결 대기
        win32pipe.ConnectNamedPipe(pipe, None)
        print("Pipe connected successfully!")
        
        while True:
            try:
                print("Waiting for data...")
                # 데이터 읽기 - 더 큰 버퍼 사용
                hr, data = win32file.ReadFile(pipe, 4096)
                
                if hr == 0:  # 성공
                    # 바이트 데이터를 문자열로 변환
                    if isinstance(data, bytes):
                        # 먼저 null 바이트 제거
                        data = data.rstrip(b'\x00')
                        
                        if len(data) == 0:
                            print("Received empty data after removing null bytes")
                            continue
                            
                        try:
                            # CP949 먼저 시도 (한글 처리)
                            msg = data.decode('cp949').strip()
                            print(f"Decoded with CP949: '{msg}'")
                        except UnicodeDecodeError:
                            try:
                                msg = data.decode('utf-8').strip()
                                print(f"Decoded with UTF-8: '{msg}'")
                            except UnicodeDecodeError:
                                try:
                                    msg = data.decode('euc-kr').strip()
                                    print(f"Decoded with EUC-KR: '{msg}'")
                                except UnicodeDecodeError:
                                    msg = data.decode('latin-1').strip()
                                    print(f"Decoded with Latin-1: '{msg}'")
                    else:
                        msg = str(data).strip()
                    
                    print(f"Received: '{msg}' (length: {len(msg)})")
                    
                    if not msg:
                        print("Empty message, continuing...")
                        continue
                    
                    # URL 처리
                    if msg.startswith("http"):
                        print(f"Fetching news content from: {msg}")
                        content = get_news_content(msg)
                        response_data = content.encode('utf-8')
                        
                    # 검색어 처리
                    else:
                        print(f"Searching news for: {msg}")
                        news_list = get_news_list(msg, display_count=15)
                        
                        if news_list:
                            response_text = '\n'.join(f"{item['title']}|{item['link']}" for item in news_list)
                            print(f"Found {len(news_list)} news articles")
                        else:
                            response_text = "검색 결과가 없습니다."
                            print("No news found")
                        
                        response_data = response_text.encode('utf-8')
                    
                    # 응답 전송
                    try:
                        win32file.WriteFile(pipe, response_data)
                        print(f"Sent response ({len(response_data)} bytes)")
                    except pywintypes.error as e:
                        print(f"WriteFile error: {e}")
                        break
                        
                else:
                    print(f"ReadFile returned error code: {hr}")
                    break
                    
            except pywintypes.error as e:
                error_code, function_name, error_message = e.args
                print(f"Pipe error: {error_code} - {error_message}")
                
                if error_code == 109:  # ERROR_BROKEN_PIPE
                    print("Pipe broken, waiting for new connection...")
                    win32file.CloseHandle(pipe)
                    
                    # 새 파이프 생성
                    pipe = win32pipe.CreateNamedPipe(
                        PIPE_NAME,
                        win32pipe.PIPE_ACCESS_DUPLEX,
                        win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE | win32pipe.PIPE_WAIT,
                        1, 65536, 65536,
                        0,
                        None
                    )
                    
                    if pipe == win32file.INVALID_HANDLE_VALUE:
                        print("Failed to recreate pipe")
                        break
                    
                    win32pipe.ConnectNamedPipe(pipe, None)
                    print("Reconnected to pipe")
                    continue
                    
                elif error_code == 232:  # ERROR_NO_DATA
                    print("Pipe closing, exiting...")
                    break
                else:
                    print(f"Unhandled pipe error: {e}")
                    break
                    
            except Exception as e:
                print(f"Unexpected error: {e}")
                try:
                    error_response = f"처리 중 오류 발생: {str(e)}"
                    win32file.WriteFile(pipe, error_response.encode('utf-8'))
                except:
                    pass
                continue
                
    except Exception as e:
        print(f"Main loop error: {e}")
    finally:
        try:
            win32file.CloseHandle(pipe)
            print("Pipe closed")
        except:
            pass

def get_news_list(category_keyword, display_count=15):
    url = "https://openapi.naver.com/v1/search/news.json"
    headers = {
        "X-Naver-Client-Id": client_id,
        "X-Naver-Client-Secret": client_secret
    }
    params = {
        "query": category_keyword,
        "display": display_count,
        "start": 1,
        "sort": "date"
    }

    try:
        print(f"Making API request for: {category_keyword}")
        response = requests.get(url, headers=headers, params=params, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            items = data.get('items', [])
            news_list = []
            
            for item in items:
                # HTML 태그 및 특수문자 제거
                title = item.get('title', '').replace("<b>", "").replace("</b>", "")
                title = title.replace("&quot;", '"').replace("&amp;", "&")
                title = title.replace("&lt;", "<").replace("&gt;", ">")
                title = title.replace("&apos;", "'")
                
                link = item.get('originallink', item.get('link', ''))
                
                if title and link:
                    news_list.append({
                        "title": title,
                        "link": link
                    })
            
            print(f"Successfully retrieved {len(news_list)} news items")
            return news_list
            
        else:
            print(f"API request failed with status code: {response.status_code}")
            print(f"Response: {response.text}")
            return []
            
    except requests.exceptions.Timeout:
        print("API request timeout")
        return []
    except requests.exceptions.RequestException as e:
        print(f"API request error: {e}")
        return []
    except Exception as e:
        print(f"Unexpected error in get_news_list: {e}")
        return []

def get_news_content(url):
    try:
        print(f"Fetching content from: {url}")
        headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
        }
        
        response = requests.get(url, timeout=15, headers=headers)
        
        if response.status_code != 200:
            return f"뉴스 본문 수집 실패 (HTTP {response.status_code})"
        
        # 인코딩 자동 감지
        response.encoding = response.apparent_encoding or 'utf-8'
        
        soup = BeautifulSoup(response.text, "html.parser")
        
        # 불필요한 요소 제거
        for element in soup(["script", "style", "nav", "header", "footer", "aside"]):
            element.decompose()
        
        # 본문 텍스트 추출
        text = soup.get_text(separator=' ', strip=True)
        
        # 텍스트 정리
        lines = text.split('\n')
        cleaned_lines = [line.strip() for line in lines if line.strip()]
        text = '\n'.join(cleaned_lines)
        
        # 길이 제한
        if len(text) > 8000:
            text = text[:8000] + "\n\n[본문이 너무 길어 일부만 표시됩니다...]"
        
        if not text.strip():
            return "뉴스 본문을 추출할 수 없습니다."
            
        print(f"Successfully extracted {len(text)} characters")
        return text
        
    except requests.exceptions.Timeout:
        return "뉴스 본문 수집 실패 (시간 초과)"
    except requests.exceptions.RequestException as e:
        return f"뉴스 본문 수집 실패 (네트워크 오류: {e})"
    except Exception as e:
        return f"뉴스 본문 수집 실패 (오류: {e})"

if __name__ == "__main__":
    main()