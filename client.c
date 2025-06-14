#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <unistd.h>

#define SIZE 256
#define RESULT 1024
int main()
{
    WSADATA wsaData;
    SOCKET sock;
    SOCKADDR_IN serverAddr;
    HANDLE hThread;
    char serverIP[100];

    printf("Server IP Enter: ");
    gets(serverIP);



    if(WSAStartup(MAKEWORD(2,2),&wsaData)!=0)
        ErrorHandling("WSAStartup() error!");
    
    sock=socket(PF_INET,SOCK_STREAM,0);
    
    memset(&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_addr.s_addr=inet_addr(serverIP);
	serverAddr.sin_port=htons(55555);

    if(connect(sock,(SOCKADDR*)&serverAddr,sizeof(serverAddr))==SOCKET_ERROR)
		ErrorHandling("connect() error");

    hThread=(HANDLE)_beginthreadex(NULL,0,HandleClient,(void*)&sock,0,NULL);

    closesocket(sock);
    WSACleanup();

    return 0;

}

unsigned WINAPI HandleClient(void* arg)
{
    SOCKET sock=*((SOCKET*)arg);
    char category[SIZE];
    char title[SIZE];
    char answer[SIZE];
    char result[RESULT];
menu:
    printf("1.정치|2.경제|3.사회|4.생활/문화|5.IT/과학|6.세계|7.종료\n");
    printf("카테고리를 입력하시오: ");
    scanf("%s",category);
    send(sock,category,strlen(category)+1,0);
TITLE:
    for(int i=0;i<15;i++)
    {
        recv(sock,title,sizeof(title),0);
        title[strlen(title)] = '\0';
        puts(title);
    }

    printf("기사 선택 또는 페이지 입력(A~O는 기사|P(숫자)는 다른 페이지 검색): ");
    scanf("%s",answer);
    send(sock,answer,strlen(answer)+1,0);
    if(answer[0] >= 'A' && answer[0]<= 'O')
    {
        goto TITLE;
    }
    else{
    recv(sock,result,sizeof(result),0);
    fputs(result,stdout);
    goto menu;
    }
}