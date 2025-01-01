#include	"unp.h"
#include  <stdbool.h>
#include  <string.h>
#include  <stdio.h>
#include  <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define port SERV_PORT
#define card_number 108
#define id_lenth 32 
#define password_lenth 64
#define message_num 50
#define round_time 60

char 	sendline[MAXLINE], recvline[MAXLINE];
char    my_id[id_lenth];
char    token[id_lenth];     //不包括換行
char    *ptr;
struct  sockaddr_in	servaddr; 
int     sockfd;


void send_request (bool already_login);
void login ();
void goto_lobby ();
void goto_room(bool random) ;
void game();


void set_terminal_size(int rows, int cols) {
    struct winsize ws;
    ws.ws_row = rows;
    ws.ws_col = cols;
    ioctl(STDOUT_FILENO, TIOCSWINSZ, &ws);
}

void send_request (bool already_login) {
    if (already_login) {
        bzero(&sendline, sizeof(sendline));
        sprintf(sendline, "%s 1\n", token);
        Writen(sockfd, sendline, strlen(sendline));
    }
    
    else {
        bzero(&sendline, sizeof(sendline));
        sprintf(sendline, "1\n");
        Writen(sockfd, sendline, strlen(sendline));
    }
    return;
}

void send_request_and_back_to_lobby(){
    send_request(true);

    bzero(&recvline, sizeof(recvline));         //代表server收到request
    Readline(sockfd, recvline, MAXLINE);
    if (recvline[0]=='4' && recvline[1]==' ') {
        printf("返回大廳中...\n");
        sleep(1);
        
    }
    else {
        printf("收到怪怪的東西：%s\n", recvline);
        exit(0);
    }

    goto_lobby();

    return;
}

void login () {
    bool valid = true;
    char id[id_lenth];
    printf("登入階段, 請輸入玩家名稱(限大小寫字母, 限長32)。\n");
    bzero(&recvline, sizeof(recvline));
    ptr = fgets(recvline, sizeof(recvline), stdin);
    recvline[strcspn(recvline, "\n")] = '\0';

    while(1){                               //輸入id
        valid = true;
        for(int i=0; i<strlen(recvline); i++){
            if(!((recvline[i]>='a' && recvline[i]<='z') || (recvline[i]>='A' && recvline[i]<='Z'))){
                printf("玩家名稱不合法, 請重新輸入玩家名稱\n");
                valid = false;
                break;
            }
        }
        if (strlen(recvline)>id_lenth) {
            printf("玩家名稱超過長度限制, 請重新輸入玩家名稱\n");
            valid = false;
        }
        if (strlen(recvline)==0) {
            printf("玩家名稱不能為空, 請重新輸入玩家名稱\n");
            valid = false;
        }
        if (valid) {
            strcpy(id, recvline);
            break;
        }
        bzero(&recvline, sizeof(recvline));
        ptr = fgets(recvline, sizeof(recvline), stdin);
        recvline[strcspn(recvline, "\n")] = '\0';
    }
////////////////////////////////////////////////////////////////////
    bzero(&sendline, sizeof(sendline));     //送登入給server
    sprintf(sendline, "2 1 %s\n", id); 
    Writen(sockfd, sendline, strlen(sendline));
    printf("登入中...玩家名稱：%s\n", id);
    sleep(1);

    bzero(&recvline, sizeof(recvline));     //收server回復 登入成功還是失敗
    Readline(sockfd, recvline, MAXLINE);

    if (strcmp(recvline, "2\n")==0) {                       //登入成功
        printf("登入成功！進入大廳中...\n");
        sleep(1);
        sprintf(my_id, "%s", id);
        bzero(&recvline, sizeof(recvline));                 //收token
        Readline(sockfd, recvline, MAXLINE);
        if (recvline[0]=='4' && recvline[1]==' ') {
            recvline[strcspn(recvline, "\n")] = '\0';              //去掉換行
            strcpy(token, &recvline[2]);
            return;
        }
        else {
            printf("收到怪怪的東西：%s\n", recvline);
            exit(0);
        }
        
    }
    else if (strcmp(recvline, "3\n")==0) {                   //登入失敗
        printf("登入失敗, 玩家名稱已存在!返回登入階段...\n");
        sleep(1);
        login();
        return;
    }
    else {
        printf("收到怪怪的東西：%s\n", recvline);
        exit(0);
    }
    
    return;
}

void goto_lobby () {
    bool random = false;
    bool open_room = true;

    printf("\033[2J\033[1;1H");
    printf("-----歡迎進入大廳-----\n");
    while(1){
        printf("輸入1: 開房間\n輸入2: 加入房間\n輸入3: 隨機匹配\n");        //讀取玩家選擇
        ptr = fgets(recvline, sizeof(recvline), stdin);
        if (strcmp(recvline, "1\n")==0 || strcmp(recvline, "2\n")==0 || strcmp(recvline, "3\n")==0) {
            break;
        }
        printf("無效輸入!\n");
        sleep(1);
    }
    
    if (strcmp(recvline, "1\n")==0) {                               //要開房間
        bzero(&sendline, sizeof(sendline));
        sprintf(sendline, "3\n");
        Writen(sockfd, sendline, strlen(sendline));
    }
    else if (strcmp(recvline, "2\n")==0) {                          //要加房間
        open_room = true;
        printf("請輸入要加入的房間號碼(0-999):\n");
        bzero(&recvline, sizeof(recvline));
        ptr = fgets(recvline, sizeof(recvline), stdin);
        recvline[strcspn(recvline, "\n")] = '\0';
    
        char room_id[MAXLINE];
        while(1) {
            int valid = true;
            if (strlen(recvline)==0) {
                printf("房號不能為空, 請重新輸入房號\n");
                valid = false;
            }
            int i = 0;
            while (recvline[i] != '\0') {
                if (!isdigit(recvline[i])) {
                    printf("房號含有非數字自元, 請重新輸入房號\n");
                    valid = false;
                    break;
                }
                i++;
            }
            int value = strtol(recvline, NULL, 10);
            if (value < 0 || value > 999) {
                printf("房號不再規定範圍內, 請重新輸入房號\n");
                valid = false;
            }
            if (valid) {
                strcpy(room_id, recvline);
                break;
            }
            bzero(&recvline, sizeof(recvline));
            ptr = fgets(recvline, sizeof(recvline), stdin);
            recvline[strcspn(recvline, "\n")] = '\0';
        }

        bzero(&sendline, sizeof(sendline));
        sprintf(sendline, "4 %s\n", room_id);
        Writen(sockfd, sendline, strlen(sendline));
    }
    else if (strcmp(recvline, "3\n")==0) {                          //要匹配
        random = true;
        bzero(&sendline, sizeof(sendline));
        sprintf(sendline, "5\n");
        Writen(sockfd, sendline, strlen(sendline));
    }


    bzero(&recvline, sizeof(recvline));     //收回復(5/16/17/18)
    Readline(sockfd, recvline, MAXLINE);
    if (recvline[0]=='5' && recvline[1]==' ') {
        printf("進入房間中...\n");
        sleep(1);
        goto_room(random);
        return;
    }
    else if (strcmp(recvline, "16\n")==0) {
        printf("查無此房, 為您重新導向大廳。\n");
        sleep(1);
        send_request_and_back_to_lobby();
        return;
    }
    else if (strcmp(recvline, "17\n")==0) {
        if (open_room || random) printf("目前房間數量已滿, 為您重新導向大廳。\n");
        else printf("此房間人數已滿, 為您重新導向大廳。\n");
        sleep(1);
        send_request_and_back_to_lobby();
        return;
    }
    else if (strcmp(recvline, "18\n")==0) {
        printf("此房間已開始遊戲, 為您重新導向大廳。\n");
        sleep(1);
        send_request_and_back_to_lobby();
        return;
    }
    else {
        printf("收到怪怪的東西：%s\n", recvline);
        exit(0);
    }

    return;
}

void goto_room(bool random) {
    char    player[9][id_lenth];
    char    *tt = NULL;
    char    room_id[id_lenth], master[id_lenth];
    int     maxfd = 0;
    fd_set  rset;


    for(int i=0; i<9; i++){
        for(int j=0; j<id_lenth; j++){
            player[i][j] = '\0';
        }
    }
    printf("\033[2J\033[1;1H");
    printf("-----歡迎進入房間-----若想退出房間回到大廳請輸入1\n");
    printf("玩家名稱: %s\n", my_id);
    recvline[strcspn(recvline, "\n")] = '\0';
    tt = strtok(recvline, " ");
    tt = strtok(NULL, " ");
    strcpy(room_id, tt);
    tt = strtok(NULL, " ");
    strcpy(master, tt);
    tt = strtok(NULL, " ");
    for(int i=0; tt != NULL; i++){
        strcpy(player[i], tt);
        tt = strtok(NULL, " ");
    }

    //把自己加入玩家名單
    if(strcmp(my_id, master)!=0) {
        for(int i=0; i<9; i++) {
            if(player[i][0]=='\0') {
                strcpy(player[i], my_id);
                break;
            }
        }
    }

    if(!random) printf ("房間號碼: %s\n", room_id);
    printf("房主: %s\n", master);
    if (player[0][0] != '\0') printf("玩家：");
    for(int i=0; i<9; i++) {
        if (player[i][0] == '\0') break;
        printf("%s ", player[i]);
    }
    printf("\n");
    
    if (strcmp(my_id, master)==0) {
        printf("您是房主, 可以隨時輸入2開始遊戲 ! ( >= 2人才可開局)\n");
    }
    
////////////////////////////////////////////////////////////////////初始輸入完畢
    FD_ZERO(&rset);
    maxfd = max(sockfd, STDIN_FILENO)+1;
    while(1) {
        FD_SET(sockfd, &rset);
        FD_SET(STDIN_FILENO, &rset);
        Select(maxfd, &rset, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &rset)) {      //收到server更新房間狀態
            bzero(&recvline, sizeof(recvline));
            Readline(sockfd, recvline, MAXLINE);
            if (recvline[0]=='9' && recvline[1]==' ') { //game start
                game();
                return;
            }
            else if (recvline[0]=='6' && recvline[1]==' ') {
                printf("有新玩家加入了, 房間狀態更新中...\n");
                sleep(1);
                for(int i=0; i<9; i++) {
                    if(player[i][0]=='\0') {
                        recvline[strcspn(recvline, "\n")] = '\0';
                        strcpy(player[i], &recvline[2]);
                        break;
                    }
                }
            }
            else if (recvline[0]=='7' && recvline[1]==' ') {
                printf("有玩家離開了, 房間狀態更新中...\n");
                sleep(1);
                recvline[strcspn(recvline, "\n")] = '\0';
                for(int i=0; i<9; i++) {
                    if(strcmp(player[i], &recvline[2])==0) {
                        sleep(2);
                        for(int j=0; j<id_lenth; j++) {
                            player[i][j]='\0';
                        }
                        break;
                    }
                }
            }
            else if (recvline[0]=='8' && recvline[1]==' ') {
                printf("房主離開了:(  房間狀態更新中...\n(請注意自己是不是新房主)\n");
                sleep(1.0);
                recvline[strcspn(recvline, "\n")] = '\0';
                strcpy(master, &recvline[2]);
                for(int i=0; i<9; i++) {
                    if(strcmp(player[i], master)==0) {
                        for(int j=0; j<id_lenth; j++) {
                            player[i][j]='\0';
                        }
                    }
                }
            }
            else {
                printf("收到怪怪的東西：%s\n", recvline);
                exit(0);
            }
            printf("\033[2J\033[1;1H");
            printf("-----歡迎進入房間-----若想退出房間回到大廳請輸入1\n");
            printf("玩家名稱: %s\n", my_id);
            if(!random) printf ("房間號碼: %s\n", room_id);
            printf("房主: %s\n", master);
            if (player[0][0] != '\0') printf("玩家：");
            for(int i=0; i<9; i++) {
                if (player[i][0] == '\0') break;
                printf("%s ", player[i]);
            }
            printf("\n");
            if(strcmp(my_id, master)==0) {
                printf("您是房主, 可以隨時輸入2開始遊戲 ! (>=2人才可開局)\n");
            }
        }
        if (FD_ISSET(STDIN_FILENO, &rset)) {                              //收到stdin(退房或開始遊戲)
            bzero(&recvline, sizeof(recvline));
            ptr = fgets(recvline, sizeof(recvline), stdin);
            if (strcmp(recvline, "1\n")==0) {           //退房
                printf("為您重新導向大廳...\n");
                bzero(&sendline, sizeof(sendline));
                sprintf(sendline, "6\n");
                Writen(sockfd, sendline, strlen(sendline));
                sleep(1);
                send_request_and_back_to_lobby();
                return;
            }
            else if (strcmp(recvline, "2\n")==0) {      //game start
                if (strcmp(my_id, master)!=0) {
                    printf("您不是房主ㄛ，不能開始遊戲。\n");
                }
                else {
                    sleep(1);
                    int people_number = 0;
                    for (int i=0; i<9; i++) {
                        if (player[i][0]!='\0') {
                            people_number++;
                        }
                    }
                    if (people_number<1) {
                        printf("人數不夠, 一個人不能玩UNO :(\n");
                    }
                    else {
                        bzero(&sendline, sizeof(sendline));
                        sprintf(sendline, "7\n");
                        Writen(sockfd, sendline, strlen(sendline));
                    }
                    ////////////////////////////
                    // game();
                    // return;
                }
            }
            else {
                printf("無效輸入!\n");
            }
        }
        
    }

}

void draw_card(int row, int col, char card[], int num){
    if(card[1]=='x'){
        if(card[0]=='x') {
            printf("\033[%d;%dH", row, col); 
            printf("┌──┐\n");
            printf("\033[%d;%dH", row+1, col);
            printf("│  │\n");
            printf("\033[%d;%dH", row+2, col); 
            printf("└──┘\n");
        }
        else if(card[0]=='d') {
            printf("\033[%d;%dH", row, col); 
            printf("\033[33m┌─\033[32m─┐\033[0m\n");
            printf("\033[%d;%dH", row+1, col);
            printf("\033[33m│\033[0m⊕ \033[31m│\033[0m\n");
            printf("\033[%d;%dH", row+2, col); 
            printf("\033[34m└─\033[31m─┘\033[0m\n");
        }
        else if(card[0]=='e'){
            printf("\033[%d;%dH", row, col); 
            printf("\033[33m┌─\033[32m─┐\033[0m\n");
            printf("\033[%d;%dH", row+1, col);
            printf("\033[33m│\033[0m+4\033[31m│\033[0m\n");
            printf("\033[%d;%dH", row+2, col); 
            printf("\033[34m└─\033[31m─┘\033[0m\n");
        }
        
    }
    
    else {
        int color = 0;
        if(card[1]=='r') color = 31;
        else if(card[1]=='y') color = 33;
        else if(card[1]=='g') color = 32;
        else if(card[1]=='b') color = 34;

        printf("\033[%d;%dH", row, col); 
        printf("\033[%dm┌──┐ \n", color);
        printf("\033[%d;%dH", row+1, col); 
        if(card[0]=='0') printf("│\033[0m 0\033[%dm│\n", color);
        else if(card[0]=='1') printf("│\033[0m 1\033[%dm│\n", color);
        else if(card[0]=='2') printf("│\033[0m 2\033[%dm│\n", color);
        else if(card[0]=='3') printf("│\033[0m 3\033[%dm│\n", color);
        else if(card[0]=='4') printf("│\033[0m 4\033[%dm│\n", color);
        else if(card[0]=='5') printf("│\033[0m 5\033[%dm│\n", color);
        else if(card[0]=='6') printf("│\033[0m 6\033[%dm│\n", color);
        else if(card[0]=='7') printf("│\033[0m 7\033[%dm│\n", color);
        else if(card[0]=='8') printf("│\033[0m 8\033[%dm│\n", color);
        else if(card[0]=='9') printf("│\033[0m 9\033[%dm│\n", color);
        else if(card[0]=='a') printf("│\033[0m⊘ \033[%dm│\n", color);
        else if(card[0]=='b') printf("│\033[0m↗↙\033[%dm│\n", color);
        else if(card[0]=='c') printf("│\033[0m+2\033[%dm│\n", color);
        else if(card[0]=='d') printf("│\033[0m⊕ \033[%dm│\n", color);
        else if(card[0]=='e') printf("│\033[0m+4\033[%dm│\n", color);
        printf("\033[%d;%dH", row+2, col); 
        printf("└──┘\033[0m  \n");
    }
    
    
    if(num>=2) {
        printf("\033[%d;%dH┐", row, col+4); 
        printf("\033[%d;%dH│", row+1, col+4); 
        printf("\033[%d;%dH┘", row+2, col+4); 
    }
    if(num>=3) {
        printf("\033[%d;%dH┐", row, col+5); 
        printf("\033[%d;%dH│", row+1, col+5); 
        printf("\033[%d;%dH┘", row+2, col+5); 
    }
    return;
}

void draw_player(int row, int col, int order, char player[], int remain, bool uno){
    printf("\033[35m\033[%d;%dH", row, col);
    if(strlen(player)>10) {
        player[7]='.';
        player[8]='.';
        player[9]='.';
        player[10]='\0';
    }
    if (remain == -1) printf("玩家已離開房間\n");
    else printf("player %d (%s)", order, player);
    printf("\033[0m");
    if(remain==1) draw_card(row+1, col+5, "xx", 1);
    else if(remain==2) draw_card(row+1, col+5, "xx", 2); 
    else draw_card(row+1, col+5, "xx", 3); 

    printf("\033[35m");
    printf("\033[%d;%dH", row+4, col+2);
    if (remain != -1) printf("剩下 %d 張\n", remain);
    if(uno) {
        printf("\033[%d;%dH", row+5, col+4);
        printf("\033[31mUNO!!!\n");
    }
    printf("\033[0m");
    return;
}

void draw_my_card(char all_card[], char new_card[]) {
                //不含換行!!!
    char book[54][3] = {"0r", "1r", "2r", "3r", "4r", "5r", "6r", "7r", "8r", "9r", "ar", "br", "cr",
                        "0y", "1y", "2y", "3y", "4y", "5y", "6y", "7y", "8y", "9y", "ay", "by", "cy",
                        "0g", "1g", "2g", "3g", "4g", "5g", "6g", "7g", "8g", "9g", "ag", "bg", "cg",
                        "0b", "1b", "2b", "3b", "4b", "5b", "6b", "7b", "8b", "9b", "ab", "bb", "cb", "dx", "ex"};
    char name[54][10] = {"紅0", "紅1", "紅2", "紅3", "紅4", "紅5", "紅6", "紅7", "紅8", "紅9", "紅禁", "紅迴", "紅+2",
                        "黃0", "黃1", "黃2", "黃3", "黃4", "黃5", "黃6", "黃7", "黃8", "黃9", "黃禁", "黃迴", "黃+2",
                        "綠0", "綠1", "綠2", "綠3", "綠4", "綠5", "綠6", "綠7", "綠8", "綠9", "綠禁", "綠迴", "綠+2",
                        "藍0", "藍1", "藍2", "藍3", "藍4", "藍5", "藍6", "藍7", "藍8", "藍9", "藍禁", "藍迴", "藍+2", "萬用", "+4"};
    int all_card_count[54];
    int new_card_count[54];
    int all_number = 0;
    int count = 0;
    for(int i=0; i<54; i++) {
        all_card_count[i] = 0;
        new_card_count[i] = 0;
    }

    char *tt;
    tt = strtok(all_card, " ");
    for (int i=0; tt != NULL; i++) {
        for (int j=0; j<54; j++) {
            if (tt[0]==book[j][0] && tt[1]==book[j][1]) {
                all_card_count[j]++;
                all_number++;
                break;
            }
        }
        tt = strtok(NULL, " ");
    }

    tt = strtok(new_card, " ");
    for (int i=0; tt != NULL; i++) {
        for (int j=0; j<54; j++) {
            if (tt[0]==book[j][0] && tt[1]==book[j][1]) {
                new_card_count[j]++;
                break;
            }
        }
        tt = strtok(NULL, " ");
    }

    
    if (all_number>33) {
        printf("\033[25;1H您的牌多到放不下...\n");
        for (int i=0; i<54; i++) {
            while (all_card_count[i]>0) {
                printf("%d:%s", count+1, name[i]);
                all_card_count[i]--;

                if (new_card_count[i]>0) {
                    printf("(new)");
                    new_card_count[i]--;
                }
                if (count < all_number-1) printf(" / ");
                
                count++;
            }
        }
    }
    else {
        for(int i=0; i<54; i++){
            while (all_card_count[i]>0) {
                if (new_card_count[i]>0) {
                    printf("\033[25;%dH new", 1+4*count);
                    new_card_count[i]--;
                }
                draw_card(26, 1+4*count, book[i], 1);
                if(count>8) printf("\033[29;%dH %d", 1+4*count, count+1);
                else printf("\033[29;%dH  %d", 1+4*count, count+1);
                all_card_count[i]--;
                count++;
            }
        }
    }
    printf("\n");
    return;
}

void draw_table (int n, char player[][id_lenth], int player_remain[], bool uno[], char broadcast[], char hint[], bool direct, char whos_turn[], 
char my_card_ori[], char new_card_ori[], int card_remain, int table_card_num, char table_card[], int time) {
    char    my_card[MAXLINE], new_card[MAXLINE];
    strcpy(my_card, my_card_ori);
    strcpy(new_card, new_card_ori);
    
    int order_idx=0, my_order=0;
    for(int i=0; i<n; i++){
        if(strcmp(player[i], whos_turn)==0) {
            order_idx = i;
        }
        if(strcmp(player[i], my_id)==0) {
            my_order = i+1;
        }
    }

    printf("\033[2J\033[1;1H");
    
    printf("\033[36m\033[1;1H\033[2K廣播:     %s\033[0m", broadcast);

    if(direct==0) printf("出牌方向: 順時針(...-> 1 -> 2 -> 3 ->...)");
    else printf("出牌方向: 逆時針(...<- 1 <- 2 <- 3...)");

    if (strcmp(my_id, whos_turn)==0) printf("\033[2;52H 輪到 player %d (%s)                      \033[31m輪到您了!\033[0m\n", order_idx+1, whos_turn);
    else printf("\033[2;52H 輪到 player %d (%s)\n", order_idx+1, whos_turn);

    printf("----------------------------------------------------------------------------------------------------------------------------------------\n");

    //更新秒數
    printf("\033[2;120H還有 %d 秒\n", time); 

    //桌上的牌
    draw_card(11, 63, table_card, table_card_num);
    printf("\033[14;58H牌堆總共 %d 張\n", table_card_num); 

    //牌庫的牌
    draw_card(11, 96, "xx", card_remain);
    printf("\033[14;91H牌庫剩下 %d 張\n", card_remain); 
    
    //draw player
    if(n==2) {
        draw_player(17, 58, my_order, player[my_order-1], player_remain[my_order-1], uno[my_order-1]);
        draw_player(4, 58, (my_order+1)%2==0 ? my_order+1 : (my_order+1)%2, player[(my_order)%2], player_remain[(my_order)%2], uno[(my_order)%2]);
    } else if(n==3) {
        draw_player(17, 58, my_order, player[my_order-1], player_remain[my_order-1], uno[my_order-1]);
        draw_player(4, 1, (my_order+1)%3==0 ? my_order+1 : (my_order+1)%3, player[(my_order)%3], player_remain[(my_order)%3], uno[(my_order)%3]);
        draw_player(4, 116, (my_order+2)%3==0 ? my_order+2 : (my_order+2)%3, player[(my_order+1)%3], player_remain[(my_order+1)%3], uno[(my_order+1)%3]);
    } else if(n==4) {
        draw_player(17, 58, my_order, player[my_order-1], player_remain[my_order-1], uno[my_order-1]);
        draw_player(10, 1, (my_order+1)%4==0 ? my_order+1 : (my_order+1)%4, player[(my_order)%4], player_remain[(my_order)%4], uno[(my_order)%4]);
        draw_player(4, 58, (my_order+2)%4==0 ? my_order+2 : (my_order+2)%4, player[(my_order+1)%4], player_remain[(my_order+1)%4], uno[(my_order+1)%4]);
        draw_player(10, 116, (my_order+3)%4==0 ? my_order+3 : (my_order+3)%4, player[(my_order+2)%4], player_remain[(my_order+2)%4], uno[(my_order+2)%4]);
    } else if (n == 5) {
        draw_player(17, 58, my_order, player[my_order - 1], player_remain[my_order - 1], uno[my_order - 1]);
        draw_player(13, 1, (my_order + 1) % 5 == 0 ? my_order + 1 : (my_order + 1) % 5, player[(my_order) % 5], player_remain[(my_order) % 5], uno[(my_order) % 5]);
        draw_player(4, 20, (my_order + 2) % 5 == 0 ? my_order + 2 : (my_order + 2) % 5, player[(my_order + 1) % 5], player_remain[(my_order + 1) % 5], uno[(my_order + 1) % 5]);
        draw_player(4, 96, (my_order + 3) % 5 == 0 ? my_order + 3 : (my_order + 3) % 5, player[(my_order + 2) % 5], player_remain[(my_order + 2) % 5], uno[(my_order + 2) % 5]);
        draw_player(13, 116, (my_order + 4) % 5 == 0 ? my_order + 4 : (my_order + 4) % 5, player[(my_order + 3) % 5], player_remain[(my_order + 3) % 5], uno[(my_order + 3) % 5]);
    } else if (n == 6) {
        draw_player(17, 58, my_order, player[my_order - 1], player_remain[my_order - 1], uno[my_order - 1]);
        draw_player(16, 15, (my_order + 1) % 6 == 0 ? my_order + 1 : (my_order + 1) % 6, player[(my_order) % 6], player_remain[(my_order) % 6], uno[(my_order) % 6]);
        draw_player(6, 1, (my_order + 2) % 6 == 0 ? my_order + 2 : (my_order + 2) % 6, player[(my_order + 1) % 6], player_remain[(my_order + 1) % 6], uno[(my_order + 1) % 6]);
        draw_player(4, 58, (my_order + 3) % 6 == 0 ? my_order + 3 : (my_order + 3) % 6, player[(my_order + 2) % 6], player_remain[(my_order + 2) % 6], uno[(my_order + 2) % 6]);
        draw_player(6, 116, (my_order + 4) % 6 == 0 ? my_order + 4 : (my_order + 4) % 6, player[(my_order + 3) % 6], player_remain[(my_order + 3) % 6], uno[(my_order + 3) % 6]);
        draw_player(16, 105, (my_order + 5) % 6 == 0 ? my_order + 5 : (my_order + 5) % 6, player[(my_order + 4) % 6], player_remain[(my_order + 4) % 6], uno[(my_order + 4) % 6]);
    } else if (n == 7) {
        draw_player(17, 58, my_order, player[my_order - 1], player_remain[my_order - 1], uno[my_order - 1]);
        draw_player(16, 10, (my_order + 1) % 7 == 0 ? my_order + 1 : (my_order + 1) % 7, player[(my_order) % 7], player_remain[(my_order) % 7], uno[(my_order) % 7]);
        draw_player(7, 1, (my_order + 2) % 7 == 0 ? my_order + 2 : (my_order + 2) % 7, player[(my_order + 1) % 7], player_remain[(my_order + 1) % 7], uno[(my_order + 1) % 7]);
        draw_player(4, 32, (my_order + 3) % 7 == 0 ? my_order + 3 : (my_order + 3) % 7, player[(my_order + 2) % 7], player_remain[(my_order + 2) % 7], uno[(my_order + 2) % 7]);
        draw_player(4, 85, (my_order + 4) % 7 == 0 ? my_order + 4 : (my_order + 4) % 7, player[(my_order + 3) % 7], player_remain[(my_order + 3) % 7], uno[(my_order + 3) % 7]);
        draw_player(7, 116, (my_order + 5) % 7 == 0 ? my_order + 5 : (my_order + 5) % 7, player[(my_order + 4) % 7], player_remain[(my_order + 4) % 7], uno[(my_order + 4) % 7]);
        draw_player(16, 110, (my_order + 6) % 7 == 0 ? my_order + 6 : (my_order + 6) % 7, player[(my_order + 5) % 7], player_remain[(my_order + 5) % 7], uno[(my_order + 5) % 7]);
    } else if (n == 8) {
        draw_player(17, 58, my_order, player[my_order - 1], player_remain[my_order - 1], uno[my_order - 1]);
        draw_player(17, 20, (my_order + 1) % 8 == 0 ? my_order + 1 : (my_order + 1) % 8, player[(my_order) % 8], player_remain[(my_order) % 8], uno[(my_order) % 8]);
        draw_player(11, 1, (my_order + 2) % 8 == 0 ? my_order + 2 : (my_order + 2) % 8, player[(my_order + 1) % 8], player_remain[(my_order + 1) % 8], uno[(my_order + 1) % 8]);
        draw_player(4, 20, (my_order + 3) % 8 == 0 ? my_order + 3 : (my_order + 3) % 8, player[(my_order + 2) % 8], player_remain[(my_order + 2) % 8], uno[(my_order + 2) % 8]);
        draw_player(4, 58, (my_order + 4) % 8 == 0 ? my_order + 4 : (my_order + 4) % 8, player[(my_order + 3) % 8], player_remain[(my_order + 3) % 8], uno[(my_order + 3) % 8]);
        draw_player(4, 96, (my_order + 5) % 8 == 0 ? my_order + 5 : (my_order + 5) % 8, player[(my_order + 4) % 8], player_remain[(my_order + 4) % 8], uno[(my_order + 4) % 8]);
        draw_player(11, 116, (my_order + 6) % 8 == 0 ? my_order + 6 : (my_order + 6) % 8, player[(my_order + 5) % 8], player_remain[(my_order + 5) % 8], uno[(my_order + 5) % 8]);
        draw_player(17, 96, (my_order + 7) % 8 == 0 ? my_order + 7 : (my_order + 7) % 8, player[(my_order + 6) % 8], player_remain[(my_order + 6) % 8], uno[(my_order + 6) % 8]);
    } else if (n == 9) {
        draw_player(17, 58, my_order, player[my_order - 1], player_remain[my_order - 1], uno[my_order - 1]);
        draw_player(17, 30, (my_order + 1) % 9 == 0 ? my_order + 1 : (my_order + 1) % 9, player[(my_order) % 9], player_remain[(my_order) % 9], uno[(my_order) % 9]);
        draw_player(13, 1, (my_order + 2) % 9 == 0 ? my_order + 2 : (my_order + 2) % 9, player[(my_order + 1) % 9], player_remain[(my_order + 1) % 9], uno[(my_order + 1) % 9]);
        draw_player(5, 10, (my_order + 3) % 9 == 0 ? my_order + 3 : (my_order + 3) % 9, player[(my_order + 2) % 9], player_remain[(my_order + 2) % 9], uno[(my_order + 2) % 9]);
        draw_player(4, 40, (my_order + 4) % 9 == 0 ? my_order + 4 : (my_order + 4) % 9, player[(my_order + 3) % 9], player_remain[(my_order + 3) % 9], uno[(my_order + 3) % 9]);
        draw_player(4, 78, (my_order + 5) % 9 == 0 ? my_order + 5 : (my_order + 5) % 9, player[(my_order + 4) % 9], player_remain[(my_order + 4) % 9], uno[(my_order + 4) % 9]);
        draw_player(5, 110, (my_order + 6) % 9 == 0 ? my_order + 6 : (my_order + 6) % 9, player[(my_order + 5) % 9], player_remain[(my_order + 5) % 9], uno[(my_order + 5) % 9]);
        draw_player(13, 116, (my_order + 7) % 9 == 0 ? my_order + 7 : (my_order + 7) % 9, player[(my_order + 6) % 9], player_remain[(my_order + 6) % 9], uno[(my_order + 6) % 9]);
        draw_player(17, 88, (my_order + 8) % 9 == 0 ? my_order + 8 : (my_order + 8) % 9, player[(my_order + 7) % 9], player_remain[(my_order + 7) % 9], uno[(my_order + 7) % 9]);
    } else if (n == 10) {
        draw_player(17, 58, my_order, player[my_order - 1], player_remain[my_order - 1], uno[my_order - 1]);
        draw_player(17, 30, (my_order + 1) % 10 == 0 ? my_order + 1 : (my_order + 1) % 10, player[(my_order) % 10], player_remain[(my_order) % 10], uno[(my_order) % 10]);
        draw_player(15, 1, (my_order + 2) % 10 == 0 ? my_order + 2 : (my_order + 2) % 10, player[(my_order + 1) % 10], player_remain[(my_order + 1) % 10], uno[(my_order + 1) % 10]);
        draw_player(6, 1, (my_order + 3) % 10 == 0 ? my_order + 3 : (my_order + 3) % 10, player[(my_order + 2) % 10], player_remain[(my_order + 2) % 10], uno[(my_order + 2) % 10]);
        draw_player(4, 30, (my_order + 4) % 10 == 0 ? my_order + 4 : (my_order + 4) % 10, player[(my_order + 3) % 10], player_remain[(my_order + 3) % 10], uno[(my_order + 3) % 10]);
        draw_player(4, 58, (my_order + 5) % 10 == 0 ? my_order + 5 : (my_order + 5) % 10, player[(my_order + 4) % 10], player_remain[(my_order + 4) % 10], uno[(my_order + 4) % 10]);
        draw_player(4, 86, (my_order + 6) % 10 == 0 ? my_order + 6 : (my_order + 6) % 10, player[(my_order + 5) % 10], player_remain[(my_order + 5) % 10], uno[(my_order + 5) % 10]);
        draw_player(6, 116, (my_order + 7) % 10 == 0 ? my_order + 7 : (my_order + 7) % 10, player[(my_order + 6) % 10], player_remain[(my_order + 6) % 10], uno[(my_order + 6) % 10]);
        draw_player(15, 116, (my_order + 8) % 10 == 0 ? my_order + 8 : (my_order + 8) % 10, player[(my_order + 7) % 10], player_remain[(my_order + 7) % 10], uno[(my_order + 7) % 10]);
        draw_player(17, 86, (my_order + 9) % 10 == 0 ? my_order + 9 : (my_order + 9) % 10, player[(my_order + 8) % 10], player_remain[(my_order + 8) % 10], uno[(my_order + 8) % 10]);
    }

    printf("\033[22;56H\n");
    printf("----------------------------------------------------------------------------------------------------------------------------------------\n");
    //最下面的規則
    printf("\033[36m\033[24;1H輸入手牌下方的數字出牌(黑卡請在後面直接加r/y/g/b) / 輸入\"skip\"跳過 / 輸入\"UNO\"喊UNO / 輸入\"UNO 玩家名稱\"誰沒喊UNO / 輸入\"gg\"投降\n\033[0m");

    draw_my_card(my_card, new_card);

    printf("\033[31;111H輸入chat進入聊天室");
    printf("\033[31;1H輸入:     / (Hint: %s)\n", hint);
    
    return;
}

int is_valid_card_input(const char *str, char *suffix) {
    int length = strlen(str);
    if (length == 0) return 0; // 空字串無效

    int i = 0;

    // 處理正負號
    if (str[i] == '-' || str[i] == '+') i++;

    // 確保有至少一個數字
    if (!isdigit(str[i])) return 0;

    // 找出數字部分
    while (isdigit(str[i])) i++;

    // 初始化後綴為空
    *suffix = '\0';

    // 檢查是否有後綴 r/y/g/b
    if (str[i] != '\0') {
        char last_char = str[i];
        if (last_char == 'r' || last_char == 'y' || last_char == 'g' || last_char == 'b') {
            *suffix = last_char; // 儲存後綴字母
            i++;
        } else {
            return 0; // 無效的後綴
        }
    }

    // 確保到達字串末尾
    if (str[i] != '\0') return 0;

    // 提取數字部分並檢查是否小於 108
    char number_part[256] = {0};
    strncpy(number_part, str, i - (*suffix ? 1 : 0));

    long value = strtol(number_part, NULL, 10);
    if (value >= 108 || value <= 0) return 0;

    return value;
}

void game() {
    //收過9了
    printf("遊戲即將開始...\n");
    sleep(1);

    int     n = 0;
    int     n_remain = 0;
    char    player[10][id_lenth];
    int     player_remain[10] = {0};
    bool    uno[10] = {false};
    char    broadcast[MAXLINE] = "~ ~ ~ Hello ~ ~ ~\n";
    char    hint[MAXLINE] = "";
    bool    direct = 0;
    char    whos_turn[id_lenth];
    char    my_card[MAXLINE] = {0};
    char    new_card[MAXLINE] = {0};
    int     card_remain = 0;            //牌庫
    int     table_card_num = 0;         //桌上的牌堆
    char    table_card[3] = {0};

    //初始化n, player list
    char *tt;
    recvline[strcspn(recvline, "\n")] = '\0';
    tt = strtok(recvline, " ");
    tt = strtok(NULL, " ");
    for(int i=0; tt != NULL; i++){
        strcpy(player[i], tt);
        tt = strtok(NULL, " ");
        n=i+1;
    }
    n_remain = n;

    int     countdown = round_time, remaining_time = round_time;
    time_t  start_time, current_time;
    int     maxfd = 0;
    fd_set  rset;
    struct timeval timeout;
    bool    renew = false;
    bool    chat = false;
    char    record[message_num][150];
    int     record_index = 0;
    for (int i = 0; i < message_num; i++) {
        record[i][0] = '\0';  // 將每一行的第一個字元設為空字元，表示空字串
    }
    
    FD_ZERO(&rset);
    maxfd = max(STDIN_FILENO, sockfd)+1;
    start_time = time(NULL);

    while (1) {
        current_time = time(NULL);
        remaining_time = countdown - (int)(current_time - start_time);
        printf("\033[s");
        if (remaining_time<=0) printf("\033[2;120H回合結束");
        else printf("\033[2;120H還有 %d 秒", remaining_time);
        printf("\033[u");
        fflush(stdout);

        if (remaining_time<=0 && (strcmp(my_id, whos_turn)==0)) {        //30s time out
            bzero(&sendline, sizeof(sendline));
            sprintf(sendline, "10\n");
            Writen(sockfd, sendline, strlen(sendline));
        }

        FD_ZERO(&rset);
        FD_SET(STDIN_FILENO, &rset);
        FD_SET(sockfd, &rset);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        Select(maxfd, &rset, NULL, NULL, &timeout);

        int     read_num;
        char    line[MAXLINE];
        char    buffer[MAXLINE];
        int     cut_count = 0;      //read index
        int     hand_count = 0;     //大家手牌總和
        bool    success = false;    //有沒有出牌成功

        if (FD_ISSET(sockfd, &rset)) {                              //收到server更新遊戲狀態
            bzero(&recvline, sizeof(recvline));
            read_num = read(sockfd, recvline, MAXLINE);
            recvline[read_num] = '\0';
            cut_count = 0;
            while(recvline[cut_count]!='\0') {
                int i = 0;
                while(1) {
                    line[i] = recvline[cut_count];
                    cut_count++;
                    i++;
                    if (recvline[cut_count-1]=='\n') {
                        line[i] = '\0';
                        break;
                    }
                }
                if (line[0]=='1' && line[1]=='0') {         //回合狀態
                    start_time = time(NULL);    //重新計時
                    renew = true;
                    line[strcspn(line, "\n")] = '\0';
                    tt = strtok(line, " ");             //10
                    tt = strtok(NULL, " ");                 //桌上的牌
                    strcpy(table_card, tt);
                    tt = strtok(NULL, " ");                 //牌庫數量
                    card_remain = strtol(tt, NULL, 10);
                    tt = strtok(NULL, " ");                 //出牌方向
                    direct = tt[0]=='1' ? true : false;
                    tt = strtok(NULL, " ");                 //輪到誰
                    strcpy(whos_turn, tt);

                    for (int i=0; i<n; i++) {               //所有人的手牌數量
                        tt = strtok(NULL, " ");             
                        player_remain[i] = strtol(tt, NULL, 10);
                        if(player_remain[i]>=2) uno[i] = 0;
                    }

                    tt = strtok(NULL, " ");                 //我的手牌
                    strcpy(my_card, "");
                    for(int i=0; tt != NULL; i++){
                        strcat(my_card, tt);
                        strcat(my_card, " ");
                        tt = strtok(NULL, " ");
                    }
                    my_card[strlen(my_card)-1] = '\0';
                }
                else if (line[0]=='1' && line[1]=='1') {    //有人亂喊UNO
                    line[strcspn(line, "\n")] = '\0';
                    tt = strtok(line, " ");             //11
                    tt = strtok(NULL, " ");                 //亂喊的id
                    sprintf(broadcast, "%s 亂喊UNO, 罰抽兩張!!!\n", tt);
                    if (strcmp(my_id, tt)) {                //不是被罰牌的人才要加
                        for (int i=0; i<n; i++) {
                            if(strcmp(tt, player[i])==0) {
                                if (player_remain[i]==1) uno[i] = 0;
                                player_remain[i] = player_remain[i] + 2;
                                card_remain = card_remain - 2;
                            }
                        }
                    }
                }
                else if (line[0]=='1' && line[1]=='2') {    //我抽到的牌
                    line[strcspn(line, "\n")] = '\0';
                    tt = strtok(line, " ");             //12
                    tt = strtok(NULL, " "); 
                    int add_num = 0;
                    for(int i=0; tt != NULL; i++){
                        if(strlen(my_card)) strcat(my_card, " ");
                        strcat(my_card, tt);
                        if(strlen(new_card)) strcat(new_card, " ");
                        strcat(new_card, tt);
                        
                        tt = strtok(NULL, " ");
                        add_num = i+1;
                    }
                    for (int i=0; i<n; i++) {
                        if(strcmp(my_id, player[i])==0) {
                            if (player_remain[i]==1) uno[i] = 0;
                            player_remain[i] = player_remain[i] + add_num;
                            card_remain = card_remain - add_num;
                        }
                    }
                }
                else if (line[0]=='2' && line[1]=='0') {    //洗牌通知
                    sprintf(broadcast, "牌抽完了，重新洗牌囉!!!\n");
                }
                else if (line[0]=='1' && line[1]=='3') {    //有人被抓到沒有喊
                    char *killer;
                    char *victim;
                    line[strcspn(line, "\n")] = '\0';
                    tt = strtok(line, " ");             //13
                    victim = strtok(NULL, " "); 
                    killer = strtok(NULL, " "); 
                    sprintf(broadcast, "%s 被 %s 抓到沒有及時喊 UNO , 罰抽兩張!!!\n", victim, killer);
                    if (strcmp(my_id, victim)) {                //不是被罰牌的人才要加
                        for (int i=0; i<n; i++) {
                            if(strcmp(victim, player[i])==0) {
                                if (player_remain[i]==1) uno[i] = 0;
                                player_remain[i] = player_remain[i] + 2;
                                card_remain = card_remain - 2;
                            }
                        }
                    }
                }
                else if (line[0]=='1' && line[1]=='9') {    //有人亂抓UNO
                    char *killer;
                    char *victim;
                    line[strcspn(line, "\n")] = '\0';
                    tt = strtok(line, " ");             //13
                    killer = strtok(NULL, " "); 
                    victim = strtok(NULL, " "); 
                    sprintf(broadcast, "%s 亂抓 %s 說他沒有喊 UNO , 罰抽兩張!!!\n", killer, victim);
                    if (strcmp(my_id, killer)) {                //不是被罰牌的人才要加
                        for (int i=0; i<n; i++) {
                            if(strcmp(killer, player[i])==0) {
                                if (player_remain[i]==1) uno[i] = 0;
                                player_remain[i] = player_remain[i] + 2;
                                card_remain = card_remain - 2;
                            }
                        }
                    }
                }
                else if (line[0]=='1' && line[1]=='4') {    //有人喊UNO
                    line[strcspn(line, "\n")] = '\0';
                    tt = strtok(line, " ");             //14
                    tt = strtok(NULL, " "); 
                    sprintf(broadcast, "%s 成功喊了UNO!!!\n", tt);
                    for (int i=0; i<n; i++) {
                        if(strcmp(tt, player[i])==0) uno[i] = true;
                    }
                }
                else if (line[0]=='7' && line[1]==' ') {    //有人退出
                    n_remain--;
                    line[strcspn(line, "\n")] = '\0';
                    tt = strtok(line, " ");             //14
                    tt = strtok(NULL, " "); 
                    sprintf(broadcast, "%s 已退出房間\n", tt);
                    for (int i=0; i<n; i++) {
                        if(strcmp(tt, player[i])==0) player_remain[i] = -1;
                    }
                }
                else if (line[0]=='1' && line[1]=='5') {    //遊戲結算
                    char *winner;
                    char *score;

                    line[strcspn(line, "\n")] = '\0';
                    tt = strtok(line, " ");             //15
                    winner = strtok(NULL, " ");
                    score = strtok(NULL, " ");

                    printf("\033[2J\033[1;1H");
                    if (n_remain == 1) printf("其他人都投降了! 你是最後贏家!!! 獲得 %s 分!\n", score);
                    else printf("遊戲結束! %s 出完了最後一張牌! 獲得 %s 分!\n", winner, score);
                    printf("即將前往大廳...\n");
                    sleep(5);

                    return;
                }
                else if (line[0]=='2' && line[1]=='1') {    //訊息
                    strcpy(record[record_index % message_num], &recvline[3]);
                    record_index++;
                }
                else {
                    printf("收到怪怪的東西：%s\n", line);
                    exit(0);
                }

                hand_count = 0;
                for (int i=0; i<n; i++) {               //所有人的手牌數量
                    if (player_remain[i] != -1) hand_count += player_remain[i];
                }
                table_card_num = card_number - card_remain - hand_count;



                if (chat) {                             //畫出聊天室
                    int tem = system("clear");                         
                    printf("hi, 這裡是聊天室! 請勿一次輸入超過100字\n");
                    for(int i=0; i<message_num; i++) {
                        if(record[(i+record_index)%message_num][0]!='\0') printf("%s", record[(i+record_index)%message_num]);
                    }
                }
                else {                                  // 更新牌桌
                    printf("\033[31;1H輸入: \n");
                    printf("\033[s");
                    draw_table(n, player, player_remain, uno, broadcast, hint, direct, whos_turn, my_card, new_card, card_remain, table_card_num, table_card, remaining_time);
                    if (renew) strcpy(new_card, "");
                    renew = false;
                    printf("\033[u");
                    fflush(stdout);
                }
            }
        }
        if (FD_ISSET(STDIN_FILENO, &rset)){ 
            bzero(&recvline, sizeof(recvline));
            ptr = fgets(recvline, sizeof(recvline), stdin);     

            if (strcmp(recvline, "chat\n")==0) {                  //聊天室
                chat = !chat;
            }
            if (chat) {
                if (strcmp(recvline, "chat\n")!=0) {                //要傳訊息
                    if (strlen(recvline)<=100) {
                        bzero(&sendline, sizeof(sendline));
                        sprintf(sendline, "21 %s: %s", my_id, recvline);
                        Writen(sockfd, sendline, strlen(sendline));
                    }
                }
                else {                                              //剛進聊天室
                    int tem = system("clear");                          //畫出聊天室
                    printf("hi, 這裡是聊天室! 請勿一次輸入超過100字\n");
                    for(int i=0; i<message_num; i++) {
                        if(record[(i+record_index)%message_num][0]!='\0') printf("%s", record[(i+record_index)%message_num]);
                    }
                }
            }
            else {
                strcpy(hint, "");                                  //清空提示               
                if (strcmp(recvline, "gg\n")==0) {                  //投降
                    bzero(&sendline, sizeof(sendline));
                    sprintf(sendline, "8\n");
                    Writen(sockfd, sendline, strlen(sendline));
                    printf("\033[2J\033[1;1H");
                    printf("您已投降，為您重新導向大廳\n");
                    sleep(3);
                    return;
                }
                else if (strcmp(recvline, "skip\n")==0) {           //skip
                    if (strcmp(my_id, whos_turn)==0) {
                        bzero(&sendline, sizeof(sendline));
                        sprintf(sendline, "10\n");
                        Writen(sockfd, sendline, strlen(sendline));
                        strcpy(hint, "您已跳過");
                    }
                    else strcpy(hint, "還沒輪到您");
                    
                }
                else if (strcmp(recvline, "UNO\n")==0) {            //UNO
                    for (int i=0; i<n; i++) {
                        if(strcmp(my_id, player[i])==0) {
                            if (player_remain[i]==1) {
                                bzero(&sendline, sizeof(sendline));
                                sprintf(sendline, "11 1 %ld\n", (long)current_time);
                                Writen(sockfd, sendline, strlen(sendline));
                            }
                            else {
                                bzero(&sendline, sizeof(sendline));
                                sprintf(sendline, "11 0 %ld\n", (long)current_time);
                                Writen(sockfd, sendline, strlen(sendline));
                            }
                            break;
                        }
                    }
                }
                else if (recvline[0]=='U' && recvline[1]=='N' && recvline[2]=='O' && recvline[3]==' ') {  //抓別人
                    recvline[strcspn(recvline, "\n")] = '\0';

                    for (int i=0; i<n; i++) {
                        if(strcmp(&recvline[4], player[i])==0) {
                            if (player_remain[i]==1) {
                                bzero(&sendline, sizeof(sendline));
                                sprintf(sendline, "12 1 %s %ld\n", player[i], (long)current_time);
                                Writen(sockfd, sendline, strlen(sendline));
                            }
                            else {
                                bzero(&sendline, sizeof(sendline));
                                sprintf(sendline, "12 0 %s %ld\n", player[i], (long)current_time);
                                Writen(sockfd, sendline, strlen(sendline));
                            }
                            break;
                        }
                    }
                }
                else {                                              //判斷能不能出牌
                    if (strcmp(my_id, whos_turn)==0) {
                        success = false;
                        recvline[strcspn(recvline, "\n")] = '\0';
                        char color;
                        int num = is_valid_card_input(recvline, &color);
                        if (!num) strcpy(hint, "無效輸入");
                        if (num) {
                            char aim_card[3] = {0};
                            char book[54][3] = {"0r", "1r", "2r", "3r", "4r", "5r", "6r", "7r", "8r", "9r", "ar", "br", "cr",
                                                "0y", "1y", "2y", "3y", "4y", "5y", "6y", "7y", "8y", "9y", "ay", "by", "cy",
                                                "0g", "1g", "2g", "3g", "4g", "5g", "6g", "7g", "8g", "9g", "ag", "bg", "cg",
                                                "0b", "1b", "2b", "3b", "4b", "5b", "6b", "7b", "8b", "9b", "ab", "bb", "cb", "dx", "ex"};
                                
                            int all_card_count[54];
                            int all_number = 0;
                            int count = 0;
                            for(int i=0; i<54; i++) {
                                all_card_count[i] = 0;
                            }

                            strcpy(buffer, my_card);
                            char *tt;
                            tt = strtok(buffer, " ");
                            for (int i=0; tt != NULL; i++) {
                                for (int j=0; j<54; j++) {
                                    if (tt[0]==book[j][0] && tt[1]==book[j][1]) {
                                        all_card_count[j]++;
                                        all_number++;
                                        break;
                                    }
                                }
                                tt = strtok(NULL, " ");
                            }
                            
                            aim_card[0]='\0';
                            aim_card[1]='\0';
                            if (num<=all_number) {
                                for (int i=0; i<54; i++) {
                                    while (all_card_count[i]>0) {
                                        count++;
                                        all_card_count[i]--;
                                        if(count==num) {
                                            aim_card[0] = book[i][0];
                                            aim_card[1] = book[i][1];
                                            break;
                                        }
                                    }
                                }
                            }
                            // printf("aim: %s\n", aim_card);
                            // sleep(5);

                            //找到是哪一張了
                            if (aim_card[0]=='d' || aim_card[0]=='e') { //黑牌
                                if (color!='\0') {
                                    bzero(&sendline, sizeof(sendline));
                                    sprintf(sendline, "9 %c%c\n", aim_card[0], color);
                                    Writen(sockfd, sendline, strlen(sendline));
                                    success = true;
                                }
                            }
                            else {
                                if (aim_card[0]==table_card[0] || aim_card[1]==table_card[1]) {
                                    bzero(&sendline, sizeof(sendline));
                                    sprintf(sendline, "9 %s\n", aim_card);
                                    Writen(sockfd, sendline, strlen(sendline));
                                    success = true;
                                }
                            }
                            if (!success) strcpy(hint, "這張牌不能出");
                        }
                    }
                    else strcpy(hint, "還沒輪到您");
                }
                //重新畫
                draw_table(n, player, player_remain, uno, broadcast, hint, direct, whos_turn, my_card, new_card, card_remain, table_card_num, table_card, remaining_time);

            }
        }
    }

    return;
}

int main (int argc, char **argv) {
    set_terminal_size(32, 156);


    bzero(&servaddr, sizeof(servaddr));
    bzero(&token, sizeof(token));
    if (argc != 2) err_quit("usage: tcpcli <IPaddress>");

    printf("連線中...\n");
    sleep(1);

    sockfd = Socket(AF_INET, SOCK_STREAM, 0);
	servaddr.sin_family = AF_INET;              //填server addr
	servaddr.sin_port = htons(port);
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    while(connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0){
        if (errno == ETIMEDOUT) {
            printf("連接逾時！正在為您重新嘗試連結, 或是Ctrl+C結束遊戲。\n");
        } else {
            perror("連接失敗(因為神秘力量:P)");
            printf("正在為您重新嘗試連結, 或是Ctrl+C結束遊戲。\n");
        }
    }

    send_request(false);         //第一次request
////////////////////////////////////////////////////////////////////  登入註冊階段
    bzero(&recvline, sizeof(recvline));         //代表server收到request 叫我登入
    Readline(sockfd, recvline, MAXLINE);
    if (strcmp(recvline, "1\n")==0) {
        printf("成功連線!");
    }
    else {
        printf("收到怪怪的東西：%s\n", recvline);
        exit(0);
    }

    login();
////////////////////////////////////////////////////////////////////  進大廳
    goto_lobby();//回來代表玩完一局 

    while(1){
        printf("\033[2J\033[1;1H");
        printf("玩完一局了!!!\n");
        sleep(1);
    ////////////////////////////////////////////////////////////////////  玩回來了
        send_request_and_back_to_lobby();
    }


    exit(0);
}