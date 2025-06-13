#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include <math.h>

#define BUF_SIZE 1024
#define NEWS_CRAW 16384
#define SIZE 256
HANDLE hMutex;
HANDLE hPipe;
DWORD WriteByte, ReadByte;
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
    recv(clientSock,category,sizeof(category),0);
    WaitForSingleObject(hMutex,INFINITE);
    WriteFile(hPipe,category,strlen(category),&WriteByte, NULL);
    ReadFile(hPipe,News_Res,sizeof(News_Res)-1,&ReadByte,NULL);
}
