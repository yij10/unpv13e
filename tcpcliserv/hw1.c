#include	"unp.h"
#include <string.h>
#include <stdlib.h>


void HomeWork(int sockfd)
{
	char sendline[MAXLINE], recvline[MAXLINE];
	char *stuid = "111550002 ", *localip;
	struct sockaddr_in localaddr;
	fd_set rset;
	// int maxfdp1;
	int addr_len = sizeof(localaddr), recv_len, n = 0;

	struct hostent *hptr;
	char *ptr, ans_ip[INET6_ADDRSTRLEN];

	// 拿自己的IP
	if(getsockname(sockfd, (struct sockaddr*)&localaddr, &addr_len) < 0)
	{
		err_quit("Faile to get local address");
	}

	localip = inet_ntoa(localaddr.sin_addr);
	
	if(localip == NULL)
	{
		printf("ntop failed\n");
	}
	printf("localip=%s\n", localip);  //debug

	sprintf(sendline, "%s", stuid);
	strcat(sendline, localip);
	strcat(sendline, "\n");

	Fputs(sendline, stdout);
	Writen(sockfd, sendline, strlen(sendline));

	FD_ZERO(&rset);
	while(1)
	{
		recv_len = read(sockfd, recvline, MAXLINE);
		if(recv_len == 0)
		{
			err_quit("str_cli: server terminated prematurely");
		}

		recvline[recv_len] = '\0';

		printf("receive: %s\n", recvline);
		// printf("%s\n", recvline);
		fflush(stdout);

		if(strcmp(recvline, "bad") == 0)
		{
			err_quit("ip not correct");
		}
		else if(strcmp(recvline, "good") == 0)
		{
			sprintf(sendline, "%s%s\n", stuid, localip);
			Writen(sockfd, sendline, strlen(sendline));
			printf("send: %s", sendline);
		}
		else if(strcmp(recvline, "nak") == 0)
		{
			err_quit("received nak");
		}
		else if(strcmp(recvline, "ok") == 0)
		{
			return ;
		}
		else
		{
			hptr = gethostbyname(recvline);
			if(hptr == NULL)
			{
				err_quit("failed to get host name");
			}
			
			ptr = hptr -> h_addr_list[0];
			if(ptr == NULL)
			{
				err_quit("ptr is NULL!");
			}

			Inet_ntop(hptr -> h_addrtype, ptr, ans_ip, sizeof(ans_ip));

			Writen(sockfd, ans_ip, strlen(ans_ip));
			printf("send: %s\n", ans_ip);
		}
		
	}
	
}	

int
main(int argc, char **argv)
{
	int					sockfd;
	struct sockaddr_in	servaddr;

	if (argc != 2)
		err_quit("usage: tcpcli <IPaddress>");

	sockfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT + 2);
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));

	HomeWork(sockfd);		/* do it all */

	exit(0);
}
