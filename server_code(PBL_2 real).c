#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define BUF_SIZE 1024
#define NEWS_CRAW 16384
#define SIZE 256
#define TOP_N 5

HANDLE hMutex;
HANDLE hPipe;
DWORD WriteByte, ReadByte;

typedef struct NEWS{
    char word[SIZE];
    int count;
    struct NEWS* next;
}NEWS;

unsigned WINAPI HandleClient(void* arg);
void Word_Count(char* news_link,SOCKET clientSock);
void Add_Word(NEWS **head, const char *word);
void Free_List(NEWS *head);
void Send_Top5_To_Client(NEWS *top5[],SOCKET clientSock);
void Find_Top5(NEWS *head, NEWS *top5[]);
void Tokenize_Korean_Words(char *text, NEWS **head);
void ErrorHandling(const char *msg);
int is_korean_word(const char *word);
int is_meaningful_word(const char *word);

int main()
{
    WSADATA wsaData;
    SOCKET serverSock,clientSock;
    SOCKADDR_IN serverAddr,clientAddr;
    int clientAddrSize;
    HANDLE hThread;
    
    // 콘솔 인코딩 설정
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    hPipe = CreateFileA(
        "\\\\.\\pipe\\NewsPipe",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (hPipe == INVALID_HANDLE_VALUE) {
        ErrorHandling("Failed to connect to pipe.");
    }

    if(WSAStartup(MAKEWORD(2,2),&wsaData)!=0)
        ErrorHandling("WSAStartup() error!");

    hMutex = CreateMutex(NULL,FALSE,NULL);
    serverSock=socket(PF_INET,SOCK_STREAM,0);

    memset(&serverAddr,0,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serverAddr.sin_port=htons(55555);

    if(bind(serverSock,(SOCKADDR*)&serverAddr,sizeof(serverAddr))==SOCKET_ERROR)
        ErrorHandling("bind() error");
    if(listen(serverSock,5)==SOCKET_ERROR)
        ErrorHandling("listen() error");
    
    while(1)
    {
        clientAddrSize = sizeof(clientAddr);
        clientSock = accept(serverSock,(SOCKADDR*)&clientAddr,&clientAddrSize);
        hThread=(HANDLE)_beginthreadex(NULL,0,HandleClient,(void*)&clientSock,0,NULL); 
    }
    closesocket(serverSock);
    WSACleanup();
    CloseHandle(hPipe);
    return 0;
}

unsigned WINAPI HandleClient(void* arg){
    SOCKET clientSock=*((SOCKET*)arg);
    char category[SIZE];
    char News_Res[NEWS_CRAW];
    char news_title[15][SIZE];
    char news_link[15][SIZE];
    char answer[SIZE];
    
menu:
    printf("클라이언트 연결 대기 중...\n");
    
    memset(category, 0, sizeof(category));
    int recv_len = recv(clientSock, category, sizeof(category)-1, 0);
    if(recv_len <= 0) {
        printf("클라이언트 연결 종료 (recv_len: %d, error: %d)\n", recv_len, WSAGetLastError());
        closesocket(clientSock);
        return 0;
    }
    
    category[recv_len] = '\0';
    
    printf("수신한 문자열: '%s' (길이: %zu)\n", category, strlen(category));
    
    if(strcmp(category,"종료") == 0) {
        printf("종료 신호 수신\n");
        closesocket(clientSock);
        return 0;
    }
    
    // 파이프를 통해 Python 프로세스에 카테고리 전송
    WaitForSingleObject(hMutex, INFINITE);
    printf("파이프로 전송: '%s'\n", category);
    
    BOOL success = WriteFile(hPipe, category, strlen(category), &WriteByte, NULL);
    if (!success) {
        DWORD error = GetLastError();
        printf("WriteFile 실패: %d\n", error);
        ReleaseMutex(hMutex);
        goto menu;
    }
    printf("WriteFile 성공 (%d bytes)\n", WriteByte);
    
    // Python에서 뉴스 목록 수신
    memset(News_Res, 0, sizeof(News_Res));
    success = ReadFile(hPipe, News_Res, sizeof(News_Res)-1, &ReadByte, NULL);
    if (!success) {
        DWORD error = GetLastError();
        printf("ReadFile 실패: %d\n", error);
        ReleaseMutex(hMutex);
        goto menu;
    }
    News_Res[ReadByte] = '\0';
    printf("ReadFile 성공 (%d bytes)\n", ReadByte);
    
    ReleaseMutex(hMutex);
    
    // 뉴스 목록 파싱
    char* line = strtok(News_Res, "\n");
    int news_count = 0;
    
    while(line != NULL && news_count < 15) {
        char* delimiter_pos = strchr(line, '|');
        if(delimiter_pos != NULL) {
            *delimiter_pos = '\0';
            char* title_part = line;
            char* link_part = delimiter_pos + 1;
            
            snprintf(news_title[news_count], sizeof(news_title[news_count]), 
                    "%c: %s", 'A' + news_count, title_part);
            strncpy(news_link[news_count], link_part, sizeof(news_link[news_count])-1);
            news_link[news_count][sizeof(news_link[news_count])-1] = '\0';
            
            printf("뉴스 %d: %s\n", news_count, news_title[news_count]);
            news_count++;
        }
        
        line = strtok(NULL, "\n");
    }
    
    printf("총 %d개의 뉴스 파싱 완료\n", news_count);
    
    // 클라이언트에게 뉴스 제목 전송
    for(int i = 0; i < news_count; i++) {
        send(clientSock, news_title[i], strlen(news_title[i]) + 1, 0);
        Sleep(10);
    }
    
    // 남은 슬롯을 빈 문자열로 채우기
    for(int i = news_count; i < 15; i++) {
        send(clientSock, "", 1, 0);
        Sleep(10);
    }

again:
    printf("클라이언트 선택 대기 중...\n");
    memset(answer, 0, sizeof(answer));
    recv_len = recv(clientSock, answer, sizeof(answer)-1, 0);
    if(recv_len <= 0) {
        printf("클라이언트 연결 종료\n");
        closesocket(clientSock);
        return 0;
    }
    answer[recv_len] = '\0';
    printf("클라이언트 선택: '%s'\n", answer);
    
    if(answer[0] >= 'A' && answer[0] <= 'O') {
        int index = answer[0] - 'A';
        if(index < news_count) {
            printf("뉴스 %d 선택: %s\n", index, news_link[index]);
            Word_Count(news_link[index], clientSock);
        } else {
            char error_msg[] = "유효하지 않은 선택입니다.";
            send(clientSock, error_msg, strlen(error_msg) + 1, 0);
        }
        goto menu;
    }
    else {
        printf("다른 검색어 입력으로 이동\n");
        goto menu;
    }
}

void Word_Count(char* news_link,SOCKET clientSock) {
    char Craw_Res[NEWS_CRAW];
    DWORD WriteByte, ReadByte;
    NEWS *head = NULL;
    NEWS *top5[TOP_N];

    printf("뉴스 링크로 본문 요청: %s\n", news_link);
    
    // 서버에 뉴스 링크 전송
    WaitForSingleObject(hMutex, INFINITE);
    WriteFile(hPipe, news_link, strlen(news_link), &WriteByte, NULL);

    // 뉴스 본문 수신
    memset(Craw_Res, 0, sizeof(Craw_Res));
    ReadFile(hPipe, Craw_Res, sizeof(Craw_Res)-1, &ReadByte, NULL);
    Craw_Res[ReadByte] = '\0';
    ReleaseMutex(hMutex);

    printf("뉴스 본문 수신 완료 (%d bytes)\n", ReadByte);
    printf("본문 일부: %.200s...\n", Craw_Res);

    // 뉴스 본문 -> 단어 분리 -> 리스트 저장
    Tokenize_Korean_Words(Craw_Res, &head);

    // 상위 5개 단어 추출
    Find_Top5(head, top5);

    // 클라이언트로 전송
    Send_Top5_To_Client(top5, clientSock);

    // 메모리 정리
    Free_List(head);
}

void Free_List(NEWS *head) {
    NEWS *temp;
    while (head) {
        temp = head;
        head = head->next;
        free(temp);
    }
}

void Send_Top5_To_Client(NEWS *top5[], SOCKET clientSock) {
    char buffer[2048] = "";
    char line[256];

    printf("=== 상위 5개 단어 ===\n");
    for (int i = 0; i < TOP_N; i++) {
        if (top5[i] && strlen(top5[i]->word) > 0) {
            printf("%d위: '%s' (%d회)\n", i + 1, top5[i]->word, top5[i]->count);
            snprintf(line, sizeof(line), "%d위: %s (%d회)\n", i + 1, top5[i]->word, top5[i]->count);
            strcat(buffer, line);
        }
    }
    printf("==================\n");
    
    if (strlen(buffer) == 0) {
        strcpy(buffer, "추출된 단어가 없습니다.\n");
    }
    
    printf("클라이언트로 전송할 데이터: %s\n", buffer);
    send(clientSock, buffer, strlen(buffer) + 1, 0);
}

void Find_Top5(NEWS *head, NEWS *top5[]) {
    for (int i = 0; i < TOP_N; i++) top5[i] = NULL;

    while (head) {
        if (is_meaningful_word(head->word)) {  // 의미있는 단어만 선택
            for (int i = 0; i < TOP_N; i++) {
                if (!top5[i] || head->count > top5[i]->count) {
                    for (int j = TOP_N - 1; j > i; j--)
                        top5[j] = top5[j - 1];
                    top5[i] = head;
                    break;
                }
            }
        }
        head = head->next;
    }
}

// 의미있는 단어인지 확인하는 함수
int is_meaningful_word(const char *word) {
    if (!word || strlen(word) < 2) return 0;  // 너무 짧은 단어 제외
    if (strlen(word) > 50) return 0;  // 너무 긴 단어 제외
    
    // 숫자만으로 구성된 단어 제외
    int digit_count = 0;
    int total_len = strlen(word);
    for (int i = 0; i < total_len; i++) {
        if (isdigit(word[i])) digit_count++;
    }
    if (digit_count == total_len) return 0;
    
    // 특수문자만으로 구성된 단어 제외
    int special_count = 0;
    for (int i = 0; i < total_len; i++) {
        if (!isalnum(word[i]) && (unsigned char)word[i] < 128) {
            special_count++;
        }
    }
    if (special_count == total_len) return 0;
    
    // 불용어 제외
    char* stopwords[] = {
        "이", "그", "저", "것", "들", "는", "은", "이", "가", "을", "를", 
        "에", "에서", "와", "과", "도", "만", "의", "로", "으로", "한", 
        "수", "등", "및", "또", "더", "매우", "정말", "너무", "아주",
        "때문", "통해", "위해", "위한", "대한", "관련", "경우", "때문에",
        "http", "https", "www", "com", "co", "kr", "html", "htm"
    };
    
    int stopword_count = sizeof(stopwords) / sizeof(stopwords[0]);
    for (int i = 0; i < stopword_count; i++) {
        if (strcmp(word, stopwords[i]) == 0) {
            return 0;
        }
    }
    
    return 1;
}

void Tokenize_Korean_Words(char *text, NEWS **head) {
    // 구분자 정의 - 공백, 문장부호, 특수문자
    char *delimiters = " \t\n\r.,!?\"()[]{}/<>:;''""\'·~`@#$%^&*+=|\\-_";
    
    // 문자열 복사본 생성 (strtok이 원본을 수정하므로)
    char *text_copy = malloc(strlen(text) + 1);
    strcpy(text_copy, text);
    
    char *token = strtok(text_copy, delimiters);
    int word_count = 0;

    while (token != NULL && word_count < 10000) {  // 최대 처리 단어 수 제한
        // 토큰 정리
        char cleaned_word[SIZE];
        int j = 0;
        
        // 앞뒤 공백 및 특수문자 제거
        int start = 0, end = strlen(token) - 1;
        while (start <= end && (token[start] == ' ' || token[start] == '\t')) start++;
        while (end >= start && (token[end] == ' ' || token[end] == '\t')) end--;
        
        for (int i = start; i <= end && j < SIZE-1; i++) {
            cleaned_word[j++] = token[i];
        }
        cleaned_word[j] = '\0';
        
        // 정리된 단어가 유효한지 확인
        if (strlen(cleaned_word) >= 2 && strlen(cleaned_word) < SIZE) {
            printf("추가할 단어: '%s'\n", cleaned_word);
            Add_Word(head, cleaned_word);
            word_count++;
        }
        
        token = strtok(NULL, delimiters);
    }
    
    free(text_copy);
    printf("총 %d개 단어 처리 완료\n", word_count);
}

void Add_Word(NEWS **head, const char *word) {
    NEWS *curr = *head;

    // 이미 존재하는 단어인지 확인
    while (curr) {
        if (strcmp(curr->word, word) == 0) {
            curr->count++;
            return;
        }
        curr = curr->next;
    }

    // 새 단어 추가
    NEWS *new_node = (NEWS *)malloc(sizeof(NEWS));
    if (new_node) {
        strncpy(new_node->word, word, SIZE-1);
        new_node->word[SIZE-1] = '\0';
        new_node->count = 1;
        new_node->next = *head;
        *head = new_node;
    }
}

void ErrorHandling(const char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}