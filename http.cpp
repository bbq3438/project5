#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <WS2tcpip.h>

#define BUFSIZE 100000

typedef struct head_parser
{
	bool CHUNKED;
	bool CONTENT_LEN;
	bool ETC;
	char *ptr;
}HEAD_PARSER;


// host name 파싱
char* parse_hostname(char *buf)
{
	char *ptr = NULL;
	char *ptr2 = NULL;
	char *buf2 = NULL;
	char name[100] = {'\0',};
	int diff;

	ptr = strstr(buf, "Host:");
	buf2 = ptr;
	ptr2 = strstr(buf2, "\r\n");

	diff = ptr2 - (ptr + 6);
	for (int i = 0; i < diff; i++)
		name[i] = *(ptr + 6 + i);

	puts(name);
	
	return name;
}


// chunked 된 패킷이면 return CHUNK
// Content_Length 패킷이면 return 사이즈
// 나머지는 return ETC
// \r\n\r\n
void reply_analysis(char *buf, HEAD_PARSER *header_parser)
{
	char header[1000] = {0,};
	char *ptr = NULL;


	// header만 분리
	ptr = strstr(buf, "\r\n\r\n");
	memcpy(header, buf, ptr - buf);
	header_parser->ptr = (ptr+4);
	

	// 내용 종류 알아내기
	ptr = NULL;
	if (ptr = strstr(header, "chunked"))
		header_parser->CHUNKED = true;
	if (ptr = strstr(header, "Content-Length:"))
		header_parser->CONTENT_LEN = true;
	header_parser->ETC = true;

}


// session별 thread 처리
DWORD WINAPI session1(void *data)
{
	SOCKET server_sock, client_sock = (SOCKET)data;
	SOCKADDR_IN client_addr, server_addr, *host_addr;
	int client_addr_len;
	char client_buf[BUFSIZE] = {0,};
	char server_buf[BUFSIZE] = {0,};
	ADDRINFO addrInfo;
	ADDRINFO *pAddrInfo;
	int ret;
	char *ptr;
	char *pEnd = NULL;
	HEAD_PARSER head_parser = { false, false, false, NULL };
	bool chunked_end = true;
	int chunked_size = 0;

	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == INVALID_SOCKET)
	{
		printf("server_sock ERROR!!\n");
		return -1;
	}

	while (1)
	{
		memset(client_buf, 0x00, BUFSIZE);
		memset(server_buf, 0x00, BUFSIZE);
		
		
		// recv()
		ret = recv(client_sock, client_buf, BUFSIZE, 0);
		if (ret == SOCKET_ERROR)
		{
			printf("recv(client -> proxy) ERROR!!\n");
			break;
		}
		else if (ret == 0)
			break;

		
		// get hostname
		ZeroMemory(&addrInfo, sizeof(addrInfo));
		addrInfo.ai_family = AF_UNSPEC;
		addrInfo.ai_socktype = SOCK_STREAM;
		addrInfo.ai_protocol = IPPROTO_TCP;

		char hostname[100] = { 0, };
		strcpy(hostname, parse_hostname(client_buf));
		if (strstr(hostname, "443") != NULL)
		{
			closesocket(server_sock);
			break;
		}
			
		getaddrinfo(hostname, "0", &addrInfo, &pAddrInfo);
		
		host_addr = (SOCKADDR_IN *)pAddrInfo->ai_addr;
		printf("%s ", inet_ntoa(host_addr->sin_addr));


		// gzip 없애기
		if((ptr = strstr(client_buf, "Encoding")) != NULL)
			strncpy(ptr, "Echangee", 8);


		// proxy -> server 보낼 내용 미리보기
		//puts(client_buf);


		// bind() : proxy -> server 
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(80);
		server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(host_addr->sin_addr));
		

		// connect() : proxy -> server
		if ((ret = connect(server_sock, (SOCKADDR *)&server_addr, sizeof(server_addr))) == SOCKET_ERROR)
		{
			printf("proxy -> server CONNECT ERROR\n");
			closesocket(server_sock);
			break;
		}
			

		// proxy -> server send()
		ret = send(server_sock, client_buf, BUFSIZE, 0);
		if (ret == SOCKET_ERROR)
		{
			printf("send(proxy -> server) ERROR!!\n");
			break;
		}
		else if (ret == 0)
		{
			printf("send(proxy -> server) < 0 > BYTE!!\n");
			break;
		}
	
		
		// server -> proxy recv()
		ret = recv(server_sock, server_buf, BUFSIZE, 0);
		if (ret == SOCKET_ERROR)
		{
			printf("recv(server -> proxy) ERROR!!\n");
			break;
		}
		else if (ret == 0)
		{
			printf("recv(server -> proxy) < 0 > BYTE!!\n");
			break;
		}
			

		// server -> proxy recv() 내용확인
		printf("\n\n");
		//puts(server_buf);













		/***********************************************************************************************
		
		 1. 헤더와 body 따로 떼어서 저장하기
		 2. 헤더부분 파싱해서 Content_Length or chunked  인지 확인하기
		 3. 새로운 버퍼에 헤더부분 넣기 (\r\n\r\n)
		 4. chunked 이면 body 부분 맨 앞 16진수로 되어있는 size 확인해서 그 사이즈만큼만 버퍼에 적재후
		    \r\n 뒤 사이즈 확인. 계속 반복하다가 0이면 버퍼 send()
		    Content_Length 이면 body의 그 사이즈만큼 버퍼에 적재후 send()
		 

		 ** 현재 상황 : 헤더와 바디 구분하는 포인터 얻기까지 성공
		                헤더부분 파싱해서 종류 확인하는것도 성공
						온전한 데이터로 만들어서 send() 하는것만 thread로 처리해주면 되는데.....
						아래의 코드는 처음에 삽질하다가 잘못된것을 깨닫고 위으 알고리즘을 다시 작성한것임
						(http에서의 데이터 흐름은 stream이라는 교수님의 말씀에 번뜩임)
						(그러나 능력의 한계에 부딪힘. 다른 시험과 같이 준비하지는 못하는 상황)
						(과제 기간은 넉넉히 주셨지만 다른 과제들처럼 며칠 밤 새면 끝날줄 알았는데 아님)
						(성적은 비록 깎이더라도 프록시 구현은 방학때 완성해 봐야겠다)

		***********************************************************************************************/













		// header 정보 초기화
		head_parser.CHUNKED = head_parser.CONTENT_LEN = head_parser.ETC = false;
		head_parser.ptr = NULL;
		chunked_end = true;

		reply_analysis(server_buf, &head_parser);
		

		// chunked인 경우
		// strtol
		if (head_parser.CHUNKED)
		{
			chunked_size = strtol(head_parser.ptr, &pEnd, 16);
			printf("%d\n", chunked_size);
			while (chunked_end)
			{
				ret = send(client_sock, server_buf, BUFSIZE, 0);
				if (ret == SOCKET_ERROR)
				{
					printf("send(proxy -> client) ERROR!!\n");
					break;
				}
				else if (ret == 0)
				{
					printf("send(proxy -> client) < 0 > BYTE!!\n");
					break;
				}
				
				//puts(server_buf);
				
				memset(client_buf, 0x00, BUFSIZE);
				memset(server_buf, 0x00, BUFSIZE);

				ret = recv(server_sock, server_buf, BUFSIZE, 0);
				if (ret == SOCKET_ERROR)
				{
					printf("recv(server -> proxy) ERROR!!\n");
					break;
				}
				else if (ret == 0)
				{
					printf("recv(server -> proxy) < 0 > BYTE!!\n");
					break;
				}
				//printf("%d 바이트 보낼것임\n", strtol(server_buf, &pEnd, 16));
				
				if ((ptr = strstr(server_buf, "\r\n0")) != NULL)
				{

				}

			}
		}
		// Content-Length 인 경우
		else if (head_parser.CONTENT_LEN)
		{
			ret = send(client_sock, server_buf, BUFSIZE, 0);
			if (ret == SOCKET_ERROR)
			{
				printf("send(proxy -> client) ERROR!!\n");
				break;
			}
			else if (ret == 0)
			{
				printf("send(proxy -> client) < 0 > BYTE!!\n");
				break;
			}
		}
		// 나머지 경우
		else if (head_parser.ETC)
		{
			ret = send(client_sock, server_buf, BUFSIZE, 0);
			if (ret == SOCKET_ERROR)
			{
				printf("send(proxy -> client) ERROR!!\n");
				break;
			}
			else if (ret == 0)
			{
				printf("send(proxy -> client) < 0 > BYTE!!\n");
				break;
			}
		}
		// ERROR 처리
		else
		{
			printf("head_parser ERROR!!\n");
			break;
		}
			
	}

	if((getpeername(client_sock, (SOCKADDR *)&client_addr, &client_addr_len)) == 0)
		printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
			inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	
	closesocket(client_sock);
	
	return 0;
}

int main()
{
	int ret;


	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return -1;


	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
	{
		printf("listen_sock ERROR!!\n");
		return -1;
	}


	// bind
	SOCKADDR_IN proxy_addr;
	ZeroMemory(&proxy_addr, sizeof(proxy_addr));
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_port = htons(8080);
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = bind(listen_sock, (SOCKADDR *)&proxy_addr, sizeof(proxy_addr));
	if (ret == SOCKET_ERROR)
	{
		printf("bind() ERROR!!\n");
		return -1;
	}


	// listen()
	ret = listen(listen_sock, SOMAXCONN);
	if (ret == SOCKET_ERROR)
	{
		printf("listen() ERROR!!\n");
		return -1;
	}


	// 데이터 통신에 사용할 변수
	HANDLE hThread;
	SOCKET client_sock;
	SOCKADDR_IN client_addr;
	int client_addr_len;

	ZeroMemory(&client_addr, sizeof(client_addr));


	// thread를 통한 멀티세션 유지
	while (1)
	{
		client_addr_len = sizeof(client_addr);
		client_sock = accept(listen_sock, (SOCKADDR *)&client_addr, &client_addr_len);
		if (client_sock == INVALID_SOCKET)
		{
			printf("accept ERROR!!\n");
			continue;
		}

		printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
			inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));


		// 요청 클라이언트별 세션관리
		hThread = CreateThread(NULL, 0, session1, (void *)client_sock, 0, NULL);
		CloseHandle(hThread);
	}

	closesocket(listen_sock);
	WSACleanup();

	return 0;
}