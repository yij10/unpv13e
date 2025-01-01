#include	"unp.h"

void worker()
{
    printf("in worker!\n");
    sleep(5);
    printf("out worker!\n");
}

int main()
{
    char recvline[100];
    int     maxfd = 0;
    fd_set  rset;

    sleep(5);
    printf("start select\n");

    FD_ZERO(&rset);
    maxfd = 1;
    while(1)
    {
        FD_SET(0, &rset);
        select(maxfd, &rset, NULL, NULL, NULL);
        if(FD_ISSET(0, &rset))
        {
            Readline(0, recvline, 100);
            printf("recv: %s\n", recvline);
            worker();
        }
    }

    
}