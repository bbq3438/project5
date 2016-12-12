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


// host name �Ľ�
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


// chunked �� ��Ŷ�̸� return CHUNK
// Content_Length ��Ŷ�̸� return ������
// �������� return ETC
// \r\n\r\n
void reply_analysis(char *buf, HEAD_PARSER *header_parser)
{
	char header[1000] = {0,};
	char *ptr = NULL;


	// header�� �и�
	ptr = strstr(buf, "\r\n\r\n");
	memcpy(header, buf, ptr - buf);
	header_parser->ptr = (ptr+4);
	

	// ���� ���� �˾Ƴ���
	ptr = NULL;
	if (ptr = strstr(header, "chunked"))
		header_parser->CHUNKED = true;
	if (ptr = strstr(header, "Content-Length:"))
		header_parser->CONTENT_LEN = true;
	header_parser->ETC = true;

}


// session�� thread ó��
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


		// gzip ���ֱ�
		if((ptr = strstr(client_buf, "Encoding")) != NULL)
			strncpy(ptr, "Echangee", 8);


		// proxy -> server ���� ���� �̸�����
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
			

		// server -> proxy recv() ����Ȯ��
		printf("\n\n");
		//puts(server_buf);













		/***********************************************************************************************
		
		 1. ����� body ���� ��� �����ϱ�
		 2. ����κ� �Ľ��ؼ� Content_Length or chunked  ���� Ȯ���ϱ�
		 3. ���ο� ���ۿ� ����κ� �ֱ� (\r\n\r\n)
		 4. chunked �̸� body �κ� �� �� 16������ �Ǿ��ִ� size Ȯ���ؼ� �� �����ŭ�� ���ۿ� ������
		    \r\n �� ������ Ȯ��. ��� �ݺ��ϴٰ� 0�̸� ���� send()
		    Content_Length �̸� body�� �� �����ŭ ���ۿ� ������ send()
		 

		 ** ���� ��Ȳ : ����� �ٵ� �����ϴ� ������ ������ ����
		                ����κ� �Ľ��ؼ� ���� Ȯ���ϴ°͵� ����
						������ �����ͷ� ���� send() �ϴ°͸� thread�� ó�����ָ� �Ǵµ�.....
						�Ʒ��� �ڵ�� ó���� �����ϴٰ� �߸��Ȱ��� ���ݰ� ���� �˰����� �ٽ� �ۼ��Ѱ���
						(http������ ������ �帧�� stream�̶�� �������� ������ ������)
						(�׷��� �ɷ��� �Ѱ迡 �ε���. �ٸ� ����� ���� �غ������� ���ϴ� ��Ȳ)
						(���� �Ⱓ�� �˳��� �ּ����� �ٸ� ������ó�� ��ĥ �� ���� ������ �˾Ҵµ� �ƴ�)
						(������ ��� ���̴��� ���Ͻ� ������ ���ж� �ϼ��� ���߰ڴ�)

		***********************************************************************************************/













		// header ���� �ʱ�ȭ
		head_parser.CHUNKED = head_parser.CONTENT_LEN = head_parser.ETC = false;
		head_parser.ptr = NULL;
		chunked_end = true;

		reply_analysis(server_buf, &head_parser);
		

		// chunked�� ���
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
				//printf("%d ����Ʈ ��������\n", strtol(server_buf, &pEnd, 16));
				
				if ((ptr = strstr(server_buf, "\r\n0")) != NULL)
				{

				}

			}
		}
		// Content-Length �� ���
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
		// ������ ���
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
		// ERROR ó��
		else
		{
			printf("head_parser ERROR!!\n");
			break;
		}
			
	}

	if((getpeername(client_sock, (SOCKADDR *)&client_addr, &client_addr_len)) == 0)
		printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	
	closesocket(client_sock);
	
	return 0;
}

int main()
{
	int ret;


	// ���� �ʱ�ȭ
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


	// ������ ��ſ� ����� ����
	HANDLE hThread;
	SOCKET client_sock;
	SOCKADDR_IN client_addr;
	int client_addr_len;

	ZeroMemory(&client_addr, sizeof(client_addr));


	// thread�� ���� ��Ƽ���� ����
	while (1)
	{
		client_addr_len = sizeof(client_addr);
		client_sock = accept(listen_sock, (SOCKADDR *)&client_addr, &client_addr_len);
		if (client_sock == INVALID_SOCKET)
		{
			printf("accept ERROR!!\n");
			continue;
		}

		printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));


		// ��û Ŭ���̾�Ʈ�� ���ǰ���
		hThread = CreateThread(NULL, 0, session1, (void *)client_sock, 0, NULL);
		CloseHandle(hThread);
	}

	closesocket(listen_sock);
	WSACleanup();

	return 0;
}