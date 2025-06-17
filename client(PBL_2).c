#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <locale.h>
#include <io.h>
#include <fcntl.h>   

#define SIZE 256
#define RESULT 1024

unsigned WINAPI HandleClient(void* arg);
void ErrorHandling(const char *msg);

int main()
{
    // 한글 출력을 위한 콘솔 설정
    SetConsoleOutputCP(65001); // UTF-8
    SetConsoleCP(949);         // CP949 입력
    
    // 로케일 설정
    setlocale(LC_ALL, "ko_KR.UTF-8");
    
    WSADATA wsaData;
    SOCKET sock;
    SOCKADDR_IN serverAddr;
    HANDLE hThread;
    char serverIP[100];

    strcpy(serverIP,"192.168.219.101");

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
    WaitForSingleObject(hThread, INFINITE);
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
    printf("뉴스 기사 단어 추출 프로그램\n");
    printf("검색하고 싶은 단어를 입력하시오: ");
    fflush(stdout);
    
    // 한글 입력 처리
    if (fgets(category, sizeof(category), stdin) != NULL) {
        // 개행문자 제거
        category[strcspn(category, "\n")] = '\0';
        
        printf("입력된 검색어: '%s'\n", category);
        
        if(strcmp(category,"종료") == 0){
            send(sock, category, strlen(category), 0);
            closesocket(sock);
            return 0;
        }
        
        // CP949로 인코딩하여 전송 (서버에서 처리하기 위해)
        int send_result = send(sock, category, strlen(category), 0);
        
        if (send_result == SOCKET_ERROR) {
            printf("전송 실패: %d\n", WSAGetLastError());
            goto menu;
        }
    } else {
        printf("입력 오류\n");
        goto menu;
    }

    printf("\n뉴스 목록을 받는 중...\n");
    printf("==========================================\n");
    
    for(int i = 0; i < 15; i++)
    {
        memset(title, 0, sizeof(title));
        int recv_len = recv(sock, title, sizeof(title) - 1, 0);
        
        if (recv_len <= 0) {
            if (i < 5) { // 처음 몇 개도 못받으면 에러
                printf("뉴스 수신 실패\n");
                goto menu;
            }
            break;
        }
        
        title[recv_len] = '\0';
        
        // 빈 문자열이 아닌 경우만 출력
        if (strlen(title) > 1) {
            // UTF-8로 출력 (콘솔이 UTF-8로 설정되어 있음)
            printf("%s\n", title);
            fflush(stdout);
        }
    }
    
    printf("==========================================\n");
    printf("뉴스 선택 (A-O) 또는 엔터(새 검색): ");
    fflush(stdout);
    
    if (fgets(answer, sizeof(answer), stdin) != NULL) {
        answer[strcspn(answer, "\n")] = '\0';  // 개행문자 제거
        
        // 빈 입력이면 새 검색으로
        if (strlen(answer) == 0) {
            goto menu;
        }
        
        int send_result = send(sock, answer, strlen(answer), 0);
        if (send_result == SOCKET_ERROR) {
            printf("선택 전송 실패\n");
            goto menu;
        }
        
        if(answer[0] >= 'A' && answer[0] <= 'O')
        {
            printf("\n뉴스 본문 분석 중...\n");
            
            // 큰 버퍼로 수신
            memset(result, 0, sizeof(result));
            int total_received = 0;
            int recv_len;
            
            // 여러 번에 나누어 수신할 수 있으므로 루프 처리
            while (total_received < sizeof(result) - 1) {
                recv_len = recv(sock, result + total_received, 
                               sizeof(result) - 1 - total_received, 0);
                
                if (recv_len <= 0) break;
                
                total_received += recv_len;
                
                // 완전한 메시지인지 확인
                if (memchr(result + total_received - recv_len, '\0', recv_len)) break;
            }
            
            if (total_received > 0) {
                result[total_received] = '\0';
                
                printf("\n=== 단어 빈도 분석 결과 ===\n");
                printf("%s", result);
                printf("========================\n\n");
                fflush(stdout);
            } else {
                printf("결과 수신 실패\n");
            }
            goto menu;
        }
        else {
            goto menu;
        }
    } else {
        printf("입력 오류\n");
        goto menu;
    }
}

void ErrorHandling(const char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}