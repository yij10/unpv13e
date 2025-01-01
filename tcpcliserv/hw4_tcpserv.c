#include	"unp.h"

void sig_chld(int sig)
{
	int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0);
}

struct connection {
	int conn, used;
	char *name;
} conns[10];

int
main()
{
	int					listenfd, connfd1, connfd2;
	pid_t				childpid;
	socklen_t			clilen1, clilen2;
	struct sockaddr_in	cliaddr1, tcpaddr, cliaddr2;
	void				sig_chld(int);

	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&tcpaddr, sizeof(tcpaddr));
	tcpaddr.sin_family      = AF_INET;
	tcpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpaddr.sin_port        = htons(SERV_PORT + 4);

	char recvline[MAXLINE + 1], sendline[MAXLINE];
	char name1[32], name2[32];
	int n;

	Bind(listenfd, (SA *) &tcpaddr, sizeof(tcpaddr));

	Listen(listenfd, LISTENQ);

	Signal(SIGCHLD, sig_chld);	/* must call waitpid() */

	for(int i = 0; i < 10; i++)	conns[i].used = 0; // 

	for ( ; ; ) {
		clilen1 = sizeof(cliaddr1);
		if ( (connfd1 = accept(listenfd, (SA *) &cliaddr1, &clilen1)) < 0) {
			if (errno == EINTR)
				continue;		/* back to for() */
			else
				err_sys("accept error");
		}

		n = Read(connfd1, recvline, MAXLINE);
		if(n == 0)
		{
			printf("user 1 didn't sent the name\n");
			exit(0);
		}
		recvline[n] = '\0';

		printf("reveice from user 1: %s\n", recvline);
		sprintf(name1, "%s", recvline);

		sprintf(sendline, "You are the 1st user. Wait for the second one!\n");
		Writen(connfd1, sendline, strlen(sendline));
		printf("send to user 1: %s", sendline);

		clilen2 = sizeof(cliaddr2);
		if ( (connfd2 = accept(listenfd, (SA *) &cliaddr2, &clilen2)) < 0) {
			if (errno == EINTR)
				continue;		/* back to for() */
			else
				err_sys("accept error");
		}
		n = Read(connfd2, recvline, MAXLINE);
		if(n == 0)
		{
			printf("user 2 didn't sent the name\n");
			exit(0);
		}
		recvline[n] = '\0';

		printf("reveice from user2: %s\n", recvline);
		sprintf(name2, "%s", recvline);

		sprintf(sendline, "You are the 2nd user.\n");
		Writen(connfd2, sendline, strlen(sendline));
		printf("send to user 2: %s", sendline);

		if ( (childpid = Fork()) == 0)
		{
			char *ip1, *ip2;

			ip1 = inet_ntoa(cliaddr1.sin_addr);
			ip2 = inet_ntoa(cliaddr2.sin_addr);

			sprintf(sendline, "The second user is %s from %s\n", name2, ip2);
			Writen(connfd1, sendline, strlen(sendline));
			printf("send to user1: %s", sendline);

			sprintf(sendline, "The first user is %s from %s\n", name1, ip1);
			Writen(connfd2, sendline, strlen(sendline));
			printf("send to user2: %s", sendline);

			fd_set rset;
			
			int maxfdp1;

			int user1_left = 0, user2_left = 0;
			while(1)
			{
				FD_ZERO(&rset);
				if(user1_left == 0) FD_SET(connfd1, &rset);
				if(user2_left == 0) FD_SET(connfd2, &rset);
				maxfdp1 = max(connfd1, connfd2) + 1;
				Select(maxfdp1, &rset, NULL, NULL, NULL);

				if(FD_ISSET(connfd1, &rset))
				{
					printf("connfd1 is ready\n");
					n = Read(connfd1, recvline, MAXLINE);
					recvline[n] = '\0';
					if(n == 0)
					{
						// Shutdown(connfd1, SHUT_RD);
						printf("user1 left\n");
						user1_left = 1;
						if(user2_left == 0)
						{
							sprintf(sendline, "(%s left the room. Press Ctrl+D to leave.)\n", name1);
							Writen(connfd2, sendline, strlen(sendline));
							printf("send to user2: %s", sendline);
							Shutdown(connfd2, SHUT_WR);
						}
						else
						{
							sprintf(sendline, "(%s left the room.)\n", name1);
							Writen(connfd2, sendline, strlen(sendline));
							printf("send tp user2: %s", sendline);

							Close(connfd1);
							Close(connfd2);
							break;
						}
					}
					else if((n == 0) && user1_left) continue;
					else
					{
						printf("receive from user1: %s\n", recvline);
						sprintf(sendline, "(%s) %s", name1, recvline);
						Writen(connfd2, sendline, strlen(sendline));
						printf("send to user2: %s\n", recvline);
					}
				}

				if(FD_ISSET(connfd2, &rset))
				{
					printf("connfd2 is ready\n");
					n = Read(connfd2, recvline, MAXLINE);
					recvline[n] = '\0';
					if(n == 0)
					{
						// Shutdown(connfd2, SHUT_RD);
						printf("user2 left\n");
						user2_left = 1;
						if(user1_left == 0)
						{
							sprintf(sendline, "(%s left the room. Press Ctrl+D to leave.)\n", name2);
							Writen(connfd1, sendline, strlen(sendline));
							printf("send to user1: %s", sendline);
							Shutdown(connfd1, SHUT_WR);
						}
						else
						{
							sprintf(sendline, "(%s left the room.)\n", name2);
							Writen(connfd1, sendline, strlen(sendline));
							printf("send tp user1: %s", sendline);

							Close(connfd1);
							Close(connfd2);
							break;
						}
					}
					else if((n == 0) && user2_left) continue;
					else
					{
						printf("receive from user2: %s\n", recvline);
						sprintf(sendline, "(%s) %s", name2, recvline);
						Writen(connfd1, sendline, strlen(sendline));
						printf("send to user1: %s\n", recvline);
					}
				}
			}
			exit(0);
		}

		Close(connfd1);			/* parent closes connected socket */
		Close(connfd2);
	}
}
