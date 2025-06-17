#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define NEWS_CRAW 16384
#define SIZE 256
#define TOP_N 5

HANDLE hMutex;
HANDLE hPipe;
DWORD WriteByte, ReadByte;

unsigned WINAPI HandleClient(void* arg);
void API_NEWS(char answer,SOCKET clientSock);
void Add_Word(NEWS **head, const char *word);
void Free_List(NEWS *head);
void Send_Top5_To_Client(NEWS *top5[],SOCKET clientSock);
void Find_Top5(NEWS *head, NEWS *top5[]);
void Tokenize_Korean_Words(char *text, NEWS **head);
void ErrorHandling(const char *msg);

typedef struct NEWS{
    char word[SIZE];
    int count;
    struct NEWS* next;
}NEWS;

int main()
{
    WSADATA wsaData;
	SOCKET serverSock,clientSock;
	SOCKADDR_IN serverAddr,clientAddr;
	int clientAddrSize;
	HANDLE hThread;
    
    

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
    char News_Res[BUF_SIZE];
    char news_title[15][SIZE];
    char news_link[15][SIZE];
    char answer[SIZE];
menu:
    recv(clientSock,category,sizeof(category),0);
    if(strcmp(category,"종료") == 0)
    {
        closesocket(clientSock);
    }
    else
    {
    WaitForSingleObject(hMutex,INFINITE);
    WriteFile(hPipe,category,strlen(category),&WriteByte, NULL);
    ReadFile(hPipe,News_Res,sizeof(News_Res)-1,&ReadByte,NULL);
    ReleaseMutex(hMutex);
    }

    char* line = strtok(News_Res, "\n");
    for(int i=0;i<15;i++)
    {
        strcpy(news_title[i],strtok(line,"|"));
        strcpy(news_link[i],strtok(NULL,"|"));

        line = strtok(NULL, "\n");
    }

    for(int i=0;i<15;i++)
    {
        send(clientSock,news_title[i],strlen(news_title[i])+1,0);
    } //이후 밑에는 페이지 또는 기사를 클라이언트에게 입력받고 그에대해 반복 또는 다른 임무 수행 진행

again:
    recv(clientSock,answer,sizeof(answer),0);
    answer[strlen(answer)+1] = '\0';
    if(answer[0] >='A'&& answer[0] <='O')
    {
        Word_Count(news_link[answer[0] - 'A'],clientSock);
        goto menu;
    }
    else {
        API_NEWS(answer,clientSock);
        goto again;
    }


}

void API_NEWS(char answer,SOCKET clientSock)
{
    int page = atoi(answer);
    char news_title[15][SIZE];
    char news_link[15][SIZE];
    char News_Res[BUF_SIZE];

    WaitForSingleObject(hMutex,INFINITE);
    WriteFile(hPipe,&page,sizeof(int),&WriteByte, NULL);
    ReadFile(hPipe,News_Res,sizeof(News_Res),&ReadByte,NULL);
    ReleaseMutex(hMutex);

    char* line = strtok(News_Res, "\n");
    for(int i=0;i<15;i++)
    {
        strcpy(news_title[i],strtok(line,"|"));
        strcpy(news_link[i],strtok(NULL,"|"));

        line = strtok(NULL, "\n");
    }

    for(int i=0;i<15;i++)
    {
        send(clientSock,news_title[i],strlen(news_title[i])+1,0);
    }
}

void Word_Count(char* news_link,SOCKET clientSock) {
    char Craw_Res[NEWS_CRAW];
    DWORD WriteByte, ReadByte;
    NEWS *head = NULL;
    NEWS *top5[TOP_N];

    // 서버에 뉴스 링크 전송
    WaitForSingleObject(hMutex, INFINITE);
    WriteFile(hPipe, news_link, strlen(news_link) + 1, &WriteByte, NULL);

    // 뉴스 본문 수신
    ReadFile(hPipe, Craw_Res, sizeof(Craw_Res), &ReadByte, NULL);
    ReleaseMutex(hMutex);

    // 뉴스 본문 -> 단어 분리 -> 리스트 저장
    Tokenize_Korean_Words(Craw_Res, &head);

    // 상위 5개 단어 추출
    Find_Top5(head, top5);

    // 클라이언트로 전송
    Send_Top5_To_Client(top5,clientSock);

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

void Send_Top5_To_Client(NEWS *top5[],SOCKET clientSock) {
    char buffer[1024] = "";
    char line[128];
    DWORD WriteByte;

    for (int i = 0; i < TOP_N; i++) {
        if (top5[i]) {
            sprintf(line, "%d위: %s (%d회)\n", i + 1, top5[i]->word, top5[i]->count);
            strcat(buffer, line);
        }
    }
    send(clientSock,buffer,strlen(buffer)+1,0);
}

void Find_Top5(NEWS *head, NEWS *top5[]) {
    for (int i = 0; i < TOP_N; i++) top5[i] = NULL;

    while (head) {
        for (int i = 0; i < TOP_N; i++) {
            if (!top5[i] || head->count > top5[i]->count) {
                for (int j = TOP_N - 1; j > i; j--)
                    top5[j] = top5[j - 1];
                top5[i] = head;
                break;
            }
        }
        head = head->next;
    }
}

void Tokenize_Korean_Words(char *text, NEWS **head) {
    char *delimiters = " \t\n\r.,!?\"()[]{}<>:;‘’“”\'";
    char *token = strtok(text, delimiters);

    while (token != NULL) {
        Add_Word(head, token);
        token = strtok(NULL, delimiters);
    }
}

void Add_Word(NEWS **head, const char *word) {
    NEWS *curr = *head;

    while (curr) {
        if (strcmp(curr->word, word) == 0) {
            curr->count++;
            return;
        }
        curr = curr->next;
    }

    NEWS *new_node = (NEWS *)malloc(sizeof(NEWS));
    strcpy(new_node->word, word);
    new_node->count = 1;
    new_node->next = *head;
    *head = new_node;
}

void ErrorHandling(const char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
