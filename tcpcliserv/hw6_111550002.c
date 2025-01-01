#include	"unp.h"

// void sig_chld(int sig)
// {
// 	int status;
//     pid_t pid;

//     while ((pid = waitpid(-1, &status, WNOHANG)) > 0);
// }

struct connection {
	int conn, used;
	char name[64];
} conns[10];

int FindEmpty()
{
    for(int i = 0; i < 10; i++)
    {
        if(conns[i].used == 0) return i;
    }

    return -1;
}

int WriteToEveryone(int who, char *sendline)
{
    // int n;
    for(int i = 0; i < 10; i++)
    {
        if(conns[i].used)
        {
            if(i != who)
            {
                Writen(conns[i].conn, sendline, strlen(sendline));
                // if(n < 0) return -1;
            }
        }
    }

    return 0;
}

int SomeoneLeave(int who, int user_num)
{
    int n;
    conns[who].used = 0;
    char sendline[MAXLINE];
    sprintf(sendline, "Bye!\n");
    Writen(conns[who].conn, sendline, strlen(sendline));
    close(conns[who].conn);
    // if(n < 0) return -1;
    if(user_num > 1) sprintf(sendline, "(%s left the room. %d users left)\n", conns[who].name, user_num);
    else sprintf(sendline, "%s left the room. You are the last one. Pres Ctrl+D to leave or wait for a new user.\n");
    for(int i = 0; i < 10; i++)
    {
        if(conns[i].used)
        {
            Writen(conns[i].conn, sendline, strlen(sendline));
            // if(n < 0) return -1;
        }
    }

    return 0;
}

int
main()
{
	int					listenfd;
	pid_t				childpid;
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, tcpaddr;
	void				sig_chld(int);

	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&tcpaddr, sizeof(tcpaddr));
	tcpaddr.sin_family      = AF_INET;
	tcpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpaddr.sin_port        = htons(SERV_PORT + 5);

	char recvline[MAXLINE + 1], sendline[MAXLINE];
	int n, maxfdp1, user_num = 0;
    fd_set rset;

	Bind(listenfd, (SA *) &tcpaddr, sizeof(tcpaddr));

	Listen(listenfd, LISTENQ);

	// Signal(SIGCHLD, sig_chld);	/* must call waitpid() */

	for(int i = 0; i < 10; i++)	conns[i].used = 0; // initial

	for ( ; ; ) {
		clilen = sizeof(cliaddr);
        
        maxfdp1 = 0;
        FD_ZERO(&rset);
        if(user_num < 10)
        {
            FD_SET(listenfd, &rset);
            if(maxfdp1 < (listenfd + 1)) maxfdp1 = listenfd + 1;
        }
        for(int i = 0; i < 10; i++)
        {
            if(conns[i].used) FD_SET(conns[i].conn, &rset);
            if(maxfdp1 < (conns[i].conn + 1)) maxfdp1 = conns[i].conn + 1;
        }

        Select(maxfdp1, &rset, NULL, NULL ,NULL);

        if(FD_ISSET(listenfd, &rset))
        {
            int index = FindEmpty();
            if(index < 0)
            {
                printf("don't have empty space\n");
                exit(0);
            }

            printf("We have %d in!\n", index);
            conns[index].used = 1;
            if ( (conns[index].conn = accept(listenfd, (SA *) &cliaddr, &clilen)) < 0) 
            {
			    if (errno == EINTR)
                    continue;		/* back to for() */
                else
                    err_sys("accept error");
            }

            printf("conn success!\n");

            n = Read(conns[index].conn, recvline, MAXLINE);
            if(n == 0)
            {
                printf("user %d didn't send the name\n", index);
                exit(0);
            }
            recvline[n] = '\0';
            sprintf(conns[index].name, "%s", recvline);
            printf("recv from %d: %s", index + 1,  recvline);

            sprintf(sendline, "You are the #%d user.\nYou may now type in or wait for other users.\n", index + 1);
            Writen(conns[index].conn, sendline, strlen(sendline));
            printf("send to user %d\n", index + 1);

            user_num++;

            sprintf(sendline, "(#%d user %s enters.)\n", index + 1, conns[index].name);
            printf("send to everyone exept %d: %s", index, sendline);
            n = WriteToEveryone(index, sendline);
            if(n < 0)
            {
                printf("somethineg wrong when write to everyone!\n");
                exit(0);
            }
        }

        for(int i = 0; i < 10; i++)
        {
            if(conns[i].used)
            {
                if(FD_ISSET(conns[i].conn, &rset))
                {
                    n = Read(conns[i].conn, recvline, MAXLINE);
                    recvline[n] = '\0';
                    if(n == 0)
                    {
                        user_num--;
                        n = SomeoneLeave(i, user_num);
                        if(n < 0)
                        {
                            printf("some one leave failed\n");
                            exit(0);
                        }
                    }
                    else
                    {
                        printf("recv from %d: %s\n", i, recvline);
                        sprintf(sendline, "(%s) %s", conns[i].name, recvline);
                        n = WriteToEveryone(i, sendline);
                        if(n < 0)
                        {
                            printf("something wrong when write to everyone\n");
                            exit(0);
                        }
                    }
                }
            }
        }

        // pid_t pid;  // clean all child sig
        // while ((pid = waitpid(-1, &status, WNOHANG)) > 0);
    }
}
