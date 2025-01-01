#include	"unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>



#define MAXPLAYER 45

const char *cards[] = {
    "0r", "0y", "0g", "0b",
    "1r", "1r", "1y", "1y", "1g", "1g", "1b", "1b",
    "2r", "2r", "2y", "2y", "2g", "2g", "2b", "2b",
    "3r", "3r", "3y", "3y", "3g", "3g", "3b", "3b",
    "4r", "4r", "4y", "4y", "4g", "4g", "4b", "4b",
    "5r", "5r", "5y", "5y", "5g", "5g", "5b", "5b",
    "6r", "6r", "6y", "6y", "6g", "6g", "6b", "6b",
    "7r", "7r", "7y", "7y", "7g", "7g", "7b", "7b",
    "8r", "8r", "8y", "8y", "8g", "8g", "8b", "8b",
    "9r", "9r", "9y", "9y", "9g", "9g", "9b", "9b",
    "ar", "ar", "ay", "ay", "ag", "ag", "ab", "ab",
    "br", "br", "by", "by", "bg", "bg", "bb", "bb",
    "cr", "cr", "cy", "cy", "cg", "cg", "cb", "cb",
    "dx", "dx", "dx", "dx", "ex", "ex", "ex", "ex"
};

const int score[] = {
    0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9,
    20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20,
    50, 50, 50, 50, 50, 50, 50, 50
};

int constent[MAXPLAYER];  // random room: 目前隨機配的room no，沒有就是-1
int room_working[MAXPLAYER], handler_working[MAXPLAYER];
pthread_t rooms[MAXPLAYER], handler[MAXPLAYER];
sem_t room_data_lock[MAXPLAYER];
int global_player_num, room_num;

// parent 用到的
struct connection {
	int connfd, state;  // player_stat = 0空的  1在大廳 2在房間
	char name[32];
} conns[MAXPLAYER];

struct link_node {
    int index;
    struct link_node *pre, *next;
} used_player_list;

struct shm_data {
    int new_player, player_num;
    int players[10];
    int used, in_game, random;
} room_data[MAXPLAYER];

struct room_thread_args {
    int room_no, owner, random;
} room_arg[MAXPLAYER], handler_arg[MAXPLAYER];

int FindEmpty()
{
    for(int i = 0; i < MAXPLAYER; i++)
    {
        if(conns[i].state == 0) return i;
    }

    return -1;
}

int AvailableToken(char *token)
{
    // 到資料庫抓
    return 1;
}

int HandleLogin(char *id)
{
    for(int i = 0; i < MAXPLAYER; i++)
    {
        if(conns[i].state == 0) continue;
        if(strcmp(id, conns[i].name) == 0) return 0;
    }
    return 1;
}

// int HandleSingnup(char *id, char *password)
// {
//     // 到資料庫抓
//     return 1;
// }


void SendToEveryone(int room_no, int who, char *sendline, int *used)
{
    char buffer[MAXLINE * 2];
    for(int i = 0; i < 10; i++)
    {
        if(i == who) continue;
        if(used[i] != -1)  // 在房間裡，要傳給他
        {
            Writen(conns[used[i]].connfd, sendline, strlen(sendline));
            // printf("room %d send: %s", room_no, sendline);
            sprintf(buffer, "room %d send to %s: %s", room_no, conns[used[i]].name, sendline);
            Writen(0, buffer, strlen(buffer));
        }
    }
}

void SendToEveryoneInGame(int room_no, int who, char *sendline, int *used, int *hand_num)
{
    char buffer[MAXLINE * 2];
    for(int i = 0; i < 10; i++)
    {
        if(i == who) continue;
        if(used[i] != -1 && hand_num[i] != -1)  // 在房間裡，要傳給他
        {
            Writen(conns[used[i]].connfd, sendline, strlen(sendline));
            // printf("room %d send: %s", room_no, sendline);
            sprintf(buffer, "room %d send to %s: %s", room_no, conns[used[i]].name, sendline);
            Writen(0, buffer, strlen(buffer));
        }
    }
}

void shuffle(int *array, int size) 
{
    for (int i = 0; i < size; i++) {
        array[i] = i;
    }

    // 初始化隨機種子
    srand((unsigned int)time(NULL));

    for (int i = size - 1; i > 0; i--) 
    {
        int j = rand() % (i + 1); // 生成 0 到 i 的隨機數
        // 交換 array[i] 和 array[j]
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void StartRound(int room_no, char *table_card, int card_num, int turn, int *used, int *hand_num, int hand[10][108], int whose_round)
{
    char sendline[MAXLINE], buffer[MAXLINE * 2], buffer2[MAXLINE * 2];
    sprintf(sendline, "10 %s %d %d %s", table_card, card_num, turn, conns[used[whose_round]].name);
    for(int i = 0; i < 10; i++)  // 接上所有人的手牌數量
    {
        if(used[i] == -1) continue;
        sprintf(buffer, " %d", hand_num[i]);
        strcat(sendline, buffer);
    }
    // sprintf(buffer2, "buffer: %s\n", buffer);
    // Writen(0, buffer2, strlen(buffer2));

    for(int i = 0; i < 10; i++)
    {
        if(used[i] == -1 || hand_num[i] == -1) continue;
        sprintf(buffer, "%s", sendline);
        for(int j = 0; j < 108; j++)
        {
            if(hand[i][j] == 0) continue;  // 找出手牌
            sprintf(buffer2, " %s", cards[j]);
            strcat(buffer, buffer2);
        }
        strcat(buffer, "\n");
        Writen(conns[used[i]].connfd, buffer, strlen(buffer));
        sprintf(buffer2, "room %d send to %s: %s", room_no, conns[used[i]].name, buffer);
        Writen(0, buffer2, strlen(buffer2));
    }
}

int SwitchToNextPlayer(int whose_round, int *hand_num, int turn)
{
    if(turn == 0)
    {
        for(int j = (whose_round + 1) % 10; j != whose_round; )
        {
            if(hand_num[j] == -1)
            {
                j = (j + 1) % 10;
                continue;
            }
            whose_round = j;
            break;
        }
    }
    else
    {
        for(int j = (whose_round + 9) % 10; j != whose_round; )
        {
            if(hand_num[j] == -1)
            {
                j = (j + 9) % 10;
                continue;
            }
            whose_round = j;
            break;
        }
    }

    return whose_round;
}

void GiveCardToPlayer(int room_no, int *card_num, int whose_round, int *used, int how_many, int *hand_num, int hand[10][108], int *card_seq, int *is_uno)
{
    char buffer[MAXLINE * 2], sendline[MAXLINE];
    int adopted_cards[108] = {0};  // 紀錄在玩家手上的牌

    sprintf(buffer, "card_num = %d\ngive card to %s\n", *card_num, conns[used[whose_round]].name);
    Writen(0, buffer, strlen(buffer));

    for(int i = 0; i < how_many; i++)
    {
        if(*card_num <= 0)  // 重新洗牌
        {
            sprintf(sendline, "20\n");
            SendToEveryoneInGame(room_no, -1, sendline, used, hand_num);
            for(int j = 0; j < 10; j++)  // 找出玩家手上的牌
            {
                if(hand_num[j] == -1) continue;
                for(int q = 0; q < 108; q++)
                {
                    if(hand[j][q] == 1) adopted_cards[q] = 1;
                }
            }

            *card_num = 0;
            for(int j = 0; j < 108; j++)
            {
                if(adopted_cards[j] == 0)
                {
                    card_seq[*card_num] = j;
                    *card_num = *card_num + 1;
                }
            }
            shuffle(card_seq, *card_num);
        }

        sprintf(buffer, "give %s(id = %d) to %s\n", cards[card_seq[*card_num]], card_seq[*card_num], conns[used[whose_round]].name);
        Writen(0, buffer, strlen(buffer)); 
        hand[whose_round][card_seq[*card_num - 1]] = 1;
        sprintf(sendline, "12 %s\n", cards[card_seq[*card_num - 1]]);
        Writen(conns[used[whose_round]].connfd, sendline, strlen(sendline));
        sprintf(buffer, "room %d send to %s: %s", room_no, conns[used[whose_round]].name, sendline);
        Writen(0, buffer, strlen(buffer));

        hand_num[whose_round]++;
        *card_num = *card_num - 1;
        if(is_uno[whose_round] == 1) is_uno[whose_round] = 0;
    }
    
}

void GameStart(int room_no, int player_num, int owner, int *used)
{
    int hand[10][108] = {0}, hand_num[10]; // 手牌: hand[player_id][i] == 1 => cards[i] 是這個人的手牌 (used[i] != -1)
    int card_seq[108], card_num;  // 記錄發牌順序，cards[card_seq[i]] 是第i 張發出去的牌，card_num 是牌堆數量
    // 從107開始發到0!!!!!!
    card_num = 108;
    shuffle(card_seq, card_num);
    int whose_round, turn, table_card, winner, winner_score;  // turn = 0 正，1 逆，更新用 turn = (turn + 1) % 2
    char colour;
    turn = 0;

    char sendline[MAXLINE], recvline[MAXLINE + 1], buffer[MAXLINE * 2], buffer2[MAXLINE * 2];

    // 發牌
    for(int i = 0; i < 10; i++)
    {
        hand_num[i] = -1;
        // for(int j = 0; j < 108; j++)
        // {
        //     hand[i][j] = -1;
        // }
    }
    for(int i = 0; i < 10; i++)
    {
        if(used[i] == -1) continue;
        for(int j = 0; j < 7; j++)
        {
            hand[i][card_seq[card_num - 1]] = 1;
            card_num--;
        }
        hand_num[i] = 7;
    }

    // 決定桌上的牌
    while(1)
    {
        table_card = card_seq[card_num - 1];
        card_num--;
        colour = cards[table_card][1];
        if(table_card < 100) break;  // 確認桌上不是萬用牌
    }

    // 抽第一個人
    srand((unsigned int)time(NULL));
    whose_round = rand() % player_num;
    for(int i = 0, j = 0; i < 10; i++)
    {
        if(used[i] == -1) continue;
        if(j == whose_round)
        {
            whose_round = i;
            break;
        }
        j++;
    }

    // 遊戲開局!!
    sprintf(sendline, "9");
    for(int i = 0; i < 10; i++)
    {
        if(used[i] == -1) continue;
        sprintf(buffer, " %s", conns[used[i]].name);
        strcat(sendline, buffer);
    }
    strcat(sendline, "\n");
    SendToEveryone(room_no, -1, sendline, used);

    // 初始回合
    sleep(1);
    sprintf(buffer, "%s", cards[table_card]);
    StartRound(room_no, buffer, card_num, turn, used, hand_num, hand, whose_round);


    // 正式開始遊戲流程
    int have_uno;  // 記有沒有人喊UNO
    long int time_stamp[10];
    int is_uno[10] = {0};  // 記這個人有沒有成功喊到uno
    char *op, *card_name, *valid, *str_time_stamp;
    int n, card_id;
    fd_set rset;
    int maxfdp1;
    while(1)
    {
        have_uno = -1;
        for(int i = 0; i < 10; i++)
        {
            time_stamp[i] = LONG_MAX;
        }
        FD_ZERO(&rset);
        maxfdp1 = 0;
        for(int i = 0; i < 10; i++)
        {
            if(used[i] == -1 || hand_num[i] == -1) continue;
            FD_SET(conns[used[i]].connfd, &rset);
            maxfdp1 = max(maxfdp1, (conns[used[i]].connfd + 1));
        }
        if(select(maxfdp1, &rset, NULL, NULL, NULL) < 0)
        {
            err_sys("select error");
        }

        for(int i = 0; i < 10; i++)  // 一個一個判斷有沒有東西
        {
            if(used[i] == -1 || hand_num[i] == -1) continue;
            if(FD_ISSET(conns[used[i]].connfd, &rset))
            {
                n = readline(conns[used[i]].connfd, recvline, MAXLINE);
                if(n == 0 || recvline[0] == '8')  // 這個人要退房! 把state 改0、hand 改-1，但要保留本來的used
                {
                    hand_num[i] = -1;
                    player_num--;

                    sprintf(sendline, "7 %s\n", conns[used[i]].name);
                    if(n == 0)
                    {
                        close(conns[used[i]].connfd);
                        conns[used[i]].state = 0;
                        global_player_num--;
                    }
                    else
                    {
                        conns[used[i]].state = 1;  // 正常退出送回大廳
                    }
                    SendToEveryoneInGame(room_no, i, sendline, used, hand_num);

                    // 如果玩家算他贏
                    if(player_num < 2)
                    {
                        for(int j = 0; j < 10; j++)
                        {
                            if(hand_num[j] != -1)
                            {
                                winner = j;
                                winner_score = 0;
                                for(int k = 0; k < 10; k++)  // 算贏家分數
                                {
                                    if(used[k] == -1) continue;
                                    for(int q = 0; q < 108; q++)
                                    {
                                        if(hand[k][q] == 1)
                                        {
                                            winner_score += score[q];
                                        }
                                    }
                                }
                                sprintf(sendline, "15 %s %d\n", conns[used[j]].name, winner_score);
                                sprintf(buffer, "room %d send to winner %s: %s", room_no, conns[used[j]].name, sendline);
                                Writen(0, buffer, strlen(buffer));
                                Writen(conns[used[j]].connfd, sendline, strlen(sendline));
                                

                                // close(conns[used[j]].connfd);
                                conns[used[j]].state = 1;
                            }
                        }
                        return ;
                    }

                    // 如果是退房的那個人的回合，要換下個人
                    if(i == whose_round)
                    {
                        whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);

                        sprintf(buffer, "%s", cards[table_card]);
                        buffer[1] = colour;
                        StartRound(room_no, buffer, card_num, turn, used, hand_num, hand, whose_round);
                    }
                    continue;
                    
                }
                else if(recvline[0] == '2' && recvline[1] == '1')  // 這個人發訊息到聊天室
                {
                    SendToEveryoneInGame(room_no, -1, recvline, used, hand_num);
                    continue;
                }

                recvline[strcspn(recvline, "\n")] = '\0';
                sprintf(buffer, "room %d recv from %s: %s\n", room_no, conns[used[i]].name, recvline);
                Writen(0, buffer, strlen(buffer));

                if(recvline[0] == '9')  // 這個人出牌，hand_num--，把對應的hand設0，如果手牌剩0他就是贏家，table_card更新，執行卡片的功能，換下一個人
                {
                    sprintf(buffer, "%s", recvline);
                    op = strtok(buffer, " ");
                    card_name = strtok(NULL, " ");
                    sprintf(buffer2, "%s", card_name);
                    if(card_name[0] == 'd' || card_name[0] == 'e')  // 萬用牌要考慮花色
                    {
                        buffer2[1] = 'x';
                    }
                    for(int j = 0; j < 108; j++)  // 找出那張牌的id
                    {
                        if(hand[i][j] == 0) continue;
                        if(strcmp(cards[j], buffer2) == 0)
                        {
                            card_id = j;
                            hand[i][j] = 0;
                            hand_num[i]--;
                            break;
                        }
                    }

                    table_card = card_id;
                    colour = card_name[1];
                    if(hand_num[i] <= 0)  // 他是贏家
                    {
                        winner = i;
                        winner_score = 0;
                        for(int k = 0; k < 10; k++)  // 算贏家分數
                        {
                            if(used[k] == -1) continue;
                            for(int q = 0; q < 108; q++)
                            {
                                if(hand[k][q] == 1)
                                {
                                    winner_score += score[q];
                                }
                            }
                        }
                        
                        sprintf(sendline, "15 %s %d\n", conns[used[winner]].name, winner_score);
                        SendToEveryoneInGame(room_no, -1, sendline, used, hand_num);

                        for(int j = 0; j < 10; j++)
                        {
                            if(hand_num[j] == -1) continue;
                            // close(conns[used[j]].connfd);
                            conns[used[j]].state = 1;
                        }
                        return ;
                    }

                    // 執行功能卡
                    if(76 <= table_card && table_card < 84)  // 禁止
                    {
                        whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                        whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                    }
                    else if(84 <= table_card && table_card < 92)  // 迴轉
                    {
                        turn = (turn + 1) % 2;
                        if(player_num > 2) whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                    }
                    else if(92 <= table_card && table_card < 100)  // +2
                    {
                        whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                        GiveCardToPlayer(room_no, &card_num, whose_round, used, 2, hand_num, hand, card_seq, is_uno);
                        whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                    }
                    else if(104 <= table_card && table_card < 108) // 萬能+4
                    {
                        whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                        GiveCardToPlayer(room_no, &card_num, whose_round, used, 4, hand_num, hand, card_seq, is_uno);
                        whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                    }
                    else
                    {
                        whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                    }
                    
                    StartRound(room_no, card_name, card_num, turn, used, hand_num, hand, whose_round);
                }
                else if(recvline[0] == '1' && recvline[1] == '0')  // skip
                {
                    GiveCardToPlayer(room_no, &card_num, whose_round, used, 1, hand_num, hand, card_seq, is_uno);
                    whose_round = SwitchToNextPlayer(whose_round, hand_num, turn);
                    sprintf(buffer, "%s", cards[table_card]);
                    buffer[1] = colour;
                    StartRound(room_no, buffer, card_num, turn, used, hand_num, hand, whose_round);
                }

                else if(recvline[0] == '1' && recvline[1] == '1')  // 這個人喊uno(自己)
                {
                    op = strtok(recvline, " ");
                    valid = strtok(NULL, " ");
                    
                    // 違法的話送兩張給他，並跟所有人廣播
                    if(valid[0] == '0')
                    {
                        GiveCardToPlayer(room_no, &card_num, i, used, 2, hand_num, hand, card_seq, is_uno);
                        sprintf(sendline, "11 %s\n", conns[used[i]].name);
                        SendToEveryoneInGame(room_no, -1, sendline, used, hand_num);
                    }
                    // 合法的話記住這回合這個人喊uno 跟time stamp，留到最後判斷
                    else
                    {
                        if(is_uno[i] == 0 && hand_num[i] == 1)  // 排除到下個select 延遲除裡的可能
                        {
                            str_time_stamp = strtok(NULL, " ");
                            have_uno = i;
                            if(time_stamp[i] == LONG_MAX) time_stamp[i] = strtol(str_time_stamp, &op, 10);
                        }
                        
                    }
                }
                else if(recvline[0] == '1' && recvline[1] == '2')  // 這個人喊別人uno
                {
                    op = strtok(recvline, " ");
                    valid = strtok(NULL, " ");
                    str_time_stamp = strtok(NULL, " ");

                    // 違法的話送兩張給他，並跟所有人廣播
                    if(valid[0] == '0')
                    {
                        GiveCardToPlayer(room_no, &card_num, i, used, 2, hand_num, hand, card_seq, is_uno);
                        sprintf(sendline, "19 %s %s\n", conns[used[i]].name, str_time_stamp);
                        SendToEveryoneInGame(room_no, -1, sendline, used, hand_num);
                    }
                    // 合法的話記住這回合這個人喊uno 跟time stamp，留到最後判斷
                    else
                    {
                        if(have_uno == -1)
                        {
                            for(int j = 0; j < 10; j++)  // 找這個id是哪個玩家的
                            {
                                if(hand_num[j] == -1) continue;
                                if(strcmp(conns[j].name, str_time_stamp) == 0)
                                {
                                    have_uno = j;
                                    break;
                                }
                            }
                        }

                        if(is_uno[have_uno] == 0 && hand_num[have_uno] == 1)  // 確保沒喊太慢到下個select
                        {
                            str_time_stamp = strtok(NULL, " ");
                            if(time_stamp[i] == LONG_MAX) time_stamp[i] = strtol(str_time_stamp, &op, 10);
                        }
                        else
                        {
                            have_uno = -1;
                        }
                    }
                }

            }

        }

        if(have_uno != -1)  // 這次select 有人喊uno!
        {
            int fastest = 0;
            for(int i = 1; i < 10; i++)  // 先找到最快的人
            {
                if(hand_num[i] == -1) continue;
                if(time_stamp[i] < time_stamp[fastest])
                {
                    fastest = i;
                }
                else if(time_stamp[i] == time_stamp[fastest])  // 剩一張的人優先
                {
                    if(i == have_uno) fastest = i;
                }
            }

            if(fastest == have_uno)  // 剩一張的人成功喊到
            {
                sprintf(buffer, "%s uno with timestamp %ld", conns[used[have_uno]].name, time_stamp[have_uno]);
                writen(0, buffer, strlen(buffer));
                is_uno[have_uno] = 1;
                sprintf(sendline, "14 %s\n", conns[used[have_uno]].name);
                SendToEveryoneInGame(room_no, -1, sendline, used, hand_num);
            }
            else  // 剩一張的人被抓了
            {
                sprintf(buffer, "%s catch %s(timestamp=%ld) with timestamp %ld", conns[used[fastest]].name, conns[used[have_uno]].name, time_stamp[have_uno], time_stamp[fastest]);
                writen(0, buffer, strlen(buffer));
                GiveCardToPlayer(room_no, &card_num, have_uno, used, 2, hand_num, hand, card_seq, is_uno);
                sprintf(sendline, "13 %s %s\n", conns[have_uno].name, conns[fastest].name);
                SendToEveryoneInGame(room_no, -1, sendline, used, hand_num);
            }
            have_uno = -1;
        }
    }
}

void *RoomProcess(void *arg)
{   // 要解散房間的時候如果new player == 1，不是random 要傳找不到房間，是random 就讓新的人當房主!!!!!
    // 千萬要記得owner 是幾p 不是conns的index
    // printf("in child\n");
    int *ptid, tid;
    ptid = (int *)arg;
    tid = *ptid;
    char buffer[MAXLINE * 2];
    fd_set rset;
    int maxfdp1;
    struct timeval timeout;
    // struct room_thread_args *args;
    // args = (struct room_thread_args *)arg;
    int room_no;
    int owner;
    int random;

    char recvline[MAXLINE + 1], sendline[MAXLINE];
    int n, used[10], player_num;  // used[] 是players[] 的備份
    int already_play;
    

    while(1)
    {
        __sync_synchronize();
        if(room_working[tid] == 0)
        {
            continue;
        } 

        for(int i = 0; i < 10; i++) used[i] = -1;
        room_no = room_arg[tid].room_no;
        owner = room_arg[tid].owner;
        random = room_arg[tid].random;
        already_play = 0;
        sprintf(buffer, "set all used to -1\n");
        Writen(0, buffer, strlen(buffer));

        while(room_working[tid] == 1)
        {
            if(already_play == 1) break;
            
            // printf("rooom %d check new player = %d\n", room_no, room_data[room_no].new_player);
            sem_wait(&room_data_lock[room_no]);
            if(room_data[room_no].new_player == 1)  // 有新的人
            {
                sprintf(buffer, "room %d %d has someone new!\n", room_no, tid);
                Writen(0, buffer, strlen(buffer));
                
                sprintf(buffer, "get sem\n");
                Writen(0, buffer, strlen(buffer));
                for(int i = 0; i < 10; i++)
                {
                    if((conns[room_data[room_no].players[i]].state == 2) && used[i] == -1)  //新的人
                    {
                        sprintf(buffer, "name: %s  index: %d, used = %d\n", conns[room_data[room_no].players[i]].name, i, used[i]);
                        Writen(0, buffer, strlen(buffer));
                        sprintf(sendline, "5 %d %s", room_no, conns[room_data[room_no].players[owner]].name);
                        for(int j = 0; j < 9; j++)
                        {
                            if((used[j] != -1) && (j != owner))
                            {
                                char buffer[33];
                                sprintf(buffer, " %s", conns[room_data[room_no].players[j]].name);
                                strcat(sendline, buffer);
                            }
                        }
                        // 讓他進房間，傳給他確認進房間的訊息
                        player_num = room_data[room_no].player_num;
                        used[i] = room_data[room_no].players[i];
                        sprintf(buffer, "room %d turn used %d to %d\n", room_no, i, room_data[room_no].players[i]);
                        Writen(0, buffer, strlen(buffer));
                        strcat(sendline, "\n");
                        Writen(conns[room_data[room_no].players[i]].connfd, sendline, strlen(sendline));
                        
                        sprintf(buffer, "room %d send to %s: %s", room_no, conns[room_data[room_no].players[i]].name, sendline);
                        Writen(0, buffer, strlen(buffer));

                        // 傳給其他人他進房間的訊息
                        sprintf(sendline, "6 %s\n", conns[room_data[room_no].players[i]].name);
                        SendToEveryone(room_no, i, sendline, used);
                    }
                }

                room_data[room_no].new_player = 0;
                
            }

            sem_post(&room_data_lock[room_no]);

            // initial select
            FD_ZERO(&rset);
            maxfdp1 = 0;
            for(int i = 0; i < 10; i++)
            {
                if(used[i] != -1)
                {
                    FD_SET(conns[used[i]].connfd, &rset);
                    maxfdp1 = max(maxfdp1, conns[used[i]].connfd + 1);
                }
            }
            timeout.tv_sec = 0;
            timeout.tv_usec = 500;
            if(select(maxfdp1, &rset, NULL, NULL ,&timeout) < 0)
            {
                err_sys("select error");
            }

            // sprintf(buffer, "selected..\n");
            // Writen(0, buffer, strlen(buffer));

            for(int i = 0; i < 10; i++)
            {
                if(used[i] == -1) continue;  // 跳過沒用到的used
                
                if(FD_ISSET(conns[used[i]].connfd, &rset))
                {
                    sprintf(buffer, "read somethine! on index %d, name: %s, sock = %d\n", i, conns[used[i]].name, conns[used[i]].connfd);
                    Writen(0, buffer, strlen(buffer));
                    n = readline(conns[used[i]].connfd, recvline, MAXLINE);
                    if(n == 0 || recvline[0] == '6')  // 他離開了
                    {
                        
                        if(n == 0)
                        {
                            close(conns[used[i]].connfd);
                            conns[used[i]].state = 0;
                            global_player_num--;
                        }
                        else
                        {
                            conns[used[i]].state = 1;  // 正常退出送回大廳
                        }
                        
                        // 有人離開!!記得把state改成0，player改-1，used改-1
                        sem_wait(&room_data_lock[room_no]);
                        room_data[room_no].players[i] = -1;
                        room_data[room_no].player_num--;
            
                        sprintf(buffer, "current mem number: %d\n", room_data[room_no].player_num);
                        Writen(0, buffer, strlen(buffer));
                        if(room_data[room_no].player_num == 0)  // 房間都沒人了
                        {
                            room_data[room_no].used = 0;
                            room_working[tid] = 0;
                            // pthread_exit(NULL);
                        }
                        player_num = room_data[room_no].player_num;
                        sem_post(&room_data_lock[room_no]);

                        if(room_working[tid] == 0) break;

                        if(i == owner)  // 萬一離開的是房主，要更新房主
                        {
                            for(int j = 0; j < 10; j++)
                            {
                                if((j != i) && (used[j] != -1))
                                {
                                    owner = j;
                                    break;
                                }
                            }
                            sprintf(sendline, "8 %s\n", conns[used[owner]].name);
                            SendToEveryone(room_no, i, sendline, used);
                        }
                        else  // 離開的不是房主就不用特別處理
                        {
                            sprintf(sendline, "7 %s\n", conns[used[i]].name);
                            SendToEveryone(room_no, i, sendline, used);
                        }
                        used[i] = -1;
                    }

                    // 接下來處理遊戲開始
                    else if(recvline[0] == '7')
                    {
                        if(player_num < 2)  // 檢查一下人夠不夠
                        {
                            sprintf(sendline, "19\n");
                            SendToEveryone(room_no, -1, sendline, used);
                        }
                        else
                        {
                            sem_wait(&room_data_lock[tid]);
                            room_data[tid].in_game = 1;
                            sem_post(&room_data_lock[tid]);
                            GameStart(room_no, player_num, owner, used);
                            already_play = 1;

                            sem_wait(&room_data_lock[tid]);
                            room_data[room_no].player_num = 0;
                            room_working[tid] = 0;
                            room_data[room_no].used = 0;
                            room_data[room_no].in_game = 0;
                            room_data[room_no].random = 0;
                            sem_post(&room_data_lock[tid]);

                            break;
                        }
                    }
                    
                }
            }
        }
        room_data[room_no].random = 0;
        room_num--;
    }

    // struct connection temp;
    // temp = room_data[room_no]->players[owner];
    // sprintf(sendline, "5 %d %s\n", room_no, room_data[room_no]->players[owner].name);
    // Writen(temp.connfd, sendline, strlen(sendline));
    // printf("room %d send: %s", room_no, sendline);
    // n = readline(room_data[room_no]->players[owner].connfd, recvline, MAXLINE);
}

void *GetInRoom(void *arg)
{
    char buffer[MAXLINE * 2];
    // struct room_thread_args *args;
    // args = (struct room_thread_args *)arg;
    int *ptid, tid;
    ptid = (int *)arg;
    tid = *ptid;
    while(1)
    {
        __sync_synchronize();
        if(handler_working[tid] == 0) continue;

        int room_no = handler_arg[tid].room_no;  // 創房間的room_no = -1，隨機的random = 1
        int player_index = handler_arg[tid].owner;
        int random = handler_arg[tid].random;

        // sprintf(buffer, "get in rooom %d check player num = %d\n", tid, room_data[room_no].player_num);
        // Writen(0, buffer, strlen(buffer));
        if(random == 1)
        {
            int success_add_room = 0;
            while(success_add_room == 0)  // 迴圈到加進房間為止
            {
                int index, fittest = -1;
                
                for(index = 0; index < MAXPLAYER; index++)
                {
                    if(room_data[index].in_game == 0 && room_data[index].random == 1)  // 找到現有的隨機房
                    {
                        break;
                    }
                    else if(room_data[index].used == 0 && fittest == -1)  // 沒有現有隨機房間就創個新的
                    {
                        fittest = index;
                    }
                }

                if(index != MAXPLAYER)  // 找到現有的隨機房
                {
                    room_no = index;

                    sem_wait(&room_data_lock[room_no]);

                    char recvline[MAXLINE + 1], sendline[MAXLINE];


                    if(room_data[room_no].used == 0)  // 房間不存在
                    {
                        sprintf(buffer, "%s want to join but random room %d close, retrying...\n", conns[player_index].name, room_no);
                        Writen(0, buffer, strlen(buffer));
                        sem_post(&room_data_lock[room_no]);
                        continue;
                    }
                    if(room_data[room_no].in_game == 1)  // 房間遊戲中
                    {
                        sprintf(buffer, "%s want to join but random room %d is in game, retrying...\n", conns[player_index].name, room_no);
                        Writen(0, buffer, strlen(buffer));
                        sem_post(&room_data_lock[room_no]);
                        continue;
                    }
                    if(room_data[room_no].player_num >= 9)  // 房間滿了
                    {
                        sprintf(buffer, "%s want to join but random room %d full, retrying...\n", conns[player_index].name, room_no);
                        Writen(0, buffer, strlen(buffer));
                        sem_post(&room_data_lock[room_no]);
                        continue;
                    }


                    int i;
                    for(i = 0; i < 10; i++)  // 找空的位置放新player 的資訊
                    {
                        if(room_data[room_no].players[i] == -1) break;
                    }
                    sprintf(buffer, "get in rooom %d put player %d to index %d\n", room_no, player_index, i);
                    Writen(0, buffer, strlen(buffer));

                    room_data[room_no].player_num++;
                    room_data[room_no].players[i] = player_index;
                    room_data[room_no].new_player = 1;
                    room_num++;

                    
                    sprintf(buffer, "child put %s(sock = %d) in room %d\n", conns[player_index].name, conns[player_index].connfd, room_no);
                    Writen(0, buffer, strlen(buffer));

                    sem_post(&room_data_lock[room_no]);

                    sprintf(buffer, "adder leave\n");
                    Writen(0, buffer, strlen(buffer));
                    handler_working[tid] = 0;
                    success_add_room = 1;
                }
                else  // 沒找到現有的隨機房間
                {
                    char sendline[MAXLINE];
                    if(fittest == -1)  // 房間數量滿了
                    {
                        sprintf(sendline, "17\n");
                        sprintf(buffer, "get in rooom %d send to %s: %s\n", room_no, conns[player_index].name, sendline);
                        Writen(0, buffer, strlen(buffer));
                        Writen(conns[player_index].connfd, sendline, strlen(sendline));
                        conns[player_index].state = 1;  // 送回大廳
                        handler_working[tid] = 0;
                        success_add_room = 1;
                        continue;
                    }
                    else  // 沒滿，開房間
                    {
                        room_no = fittest;
                        sem_wait(&room_data_lock[room_no]);
                        if(room_data[room_no].used == 1) 
                        {
                            sem_post(&room_data_lock[room_no]);
                            continue;
                        }
                        room_data[fittest].used = 1;
                        room_data[fittest].player_num = 1;
                        room_data[fittest].random = 1;
                        room_data[fittest].players[0] = player_index;
                        // memcpy(&room_data[index]->players[0], &conns[temp->index], sizeof(struct connection));
                        room_data[fittest].in_game = 0;
                        room_data[fittest].new_player = 1;
                        // conns[player_index].state = 2;
                        room_num++;

                        // printf("index + %d\n", index);
                        // 開房間的thread;
                        // struct room_thread_args args;
                        room_arg[fittest].owner = 0;
                        room_arg[fittest].room_no = fittest;
                        room_arg[fittest].random = 1;
                        room_working[fittest] = 1;
                        sem_post(&room_data_lock[room_no]);
                        success_add_room = 1;
                        handler_working[tid] = 0;
                        continue;
                    }
                }
            }
        }

        else if(room_no == -1)  // 建房間
        {
            char sendline[MAXLINE];
            int success_add_room = 0;
            while(success_add_room == 0)  // 迴圈到加進房間為止
            {
                int index;
                
                for(index = 0; index < MAXPLAYER; index++)
                {
                    if(room_data[index].used == 0)  // 找到現有的隨機房
                    {
                        break;
                    }
                }
                if(index == MAXPLAYER)  // 房間數量滿了
                {
                    sprintf(sendline, "17\n");
                    sprintf(buffer, "get in rooom %d send to %s: %s\n", room_no, conns[player_index].name, sendline);
                    Writen(0, buffer, strlen(buffer));
                    Writen(conns[player_index].connfd, sendline, strlen(sendline));
                    conns[player_index].state = 1;  // 送回大廳
                    handler_working[tid] = 0;
                    success_add_room = 1;
                    continue;
                }
                else  // 沒滿，開房間
                {
                    room_no = index;
                    sem_wait(&room_data_lock[room_no]);
                    if(room_data[room_no].used == 1) 
                    {
                        sem_post(&room_data_lock[room_no]);
                        continue;
                    }
                    room_data[room_no].used = 1;
                    room_data[room_no].player_num = 1;
                    room_data[room_no].random = 0;
                    room_data[room_no].players[0] = player_index;
                    // memcpy(&room_data[index]->players[0], &conns[temp->index], sizeof(struct connection));
                    room_data[room_no].in_game = 0;
                    room_data[room_no].new_player = 1;
                    // conns[player_index].state = 2;
                    room_num++;

                    // printf("index + %d\n", index);
                    // 開房間的thread;
                    // struct room_thread_args args;
                    room_arg[room_no].owner = 0;
                    room_arg[room_no].room_no = room_no;
                    room_arg[room_no].random = 1;
                    room_working[room_no] = 1;
                    sem_post(&room_data_lock[room_no]);
                    success_add_room = 1;
                    handler_working[tid] = 0;
                    continue;
                }

            }
        }

        else
        {
            sem_wait(&room_data_lock[room_no]);

            char recvline[MAXLINE + 1], sendline[MAXLINE];


            if((room_data[room_no].used == 0) || (room_data[room_no].random == 1))  // 房間不存在
            {
                sprintf(sendline, "16\n");
                Writen(conns[player_index].connfd, sendline, strlen(sendline));
                
                sprintf(buffer, "child send: %s", sendline);
                Writen(0, buffer, strlen(buffer));
                sem_post(&room_data_lock[room_no]);
                handler_working[tid] = 0;
                conns[player_index].state = 1;  // 送回main
                continue;
                // pthread_exit(NULL);
            }
            if(room_data[room_no].in_game == 1)  // 房間遊戲中
            {
                sprintf(sendline, "18\n");
                Writen(conns[player_index].connfd, sendline, strlen(sendline));
                
                sprintf(buffer, "child send: %s", sendline);
                Writen(0, buffer, strlen(buffer));
                sem_post(&room_data_lock[room_no]);
                handler_working[tid] = 0;
                conns[player_index].state = 1;  // 送回main
                continue;
                // pthread_exit(NULL);
            }
            if(room_data[room_no].player_num >= 9)  // 房間滿了
            {
                sprintf(sendline, "17\n");
                Writen(conns[player_index].connfd, sendline, strlen(sendline));
                
                sprintf(buffer, "child send: %s", sendline);
                Writen(0, buffer, strlen(buffer));
                sem_post(&room_data_lock[room_no]);
                handler_working[tid] = 0;
                conns[player_index].state = 1;  // 送回main
                continue;
                // pthread_exit(NULL);
            }



            int i;
            for(i = 0; i < 10; i++)  // 找空的位置放新player 的資訊
            {
                if(room_data[room_no].players[i] == -1) break;
            }
            sprintf(buffer, "get in rooom %d put player %d to index %d\n", room_no, player_index, i);
            Writen(0, buffer, strlen(buffer));

            room_data[room_no].player_num++;
            room_data[room_no].players[i] = player_index;
            room_data[room_no].new_player = 1;
            room_num++;
            

            
            sprintf(buffer, "child put %s(sock = %d) in room %d\n", conns[player_index].name, conns[player_index].connfd, room_no);
            Writen(0, buffer, strlen(buffer));

            sem_post(&room_data_lock[room_no]);

            sprintf(buffer, "adder leave\n");
            Writen(0, buffer, strlen(buffer));
            handler_working[tid] = 0;
        }
        
    }
    
}

int
main()
{
    char buffer[MAXLINE * 2];
	int					listenfd, n;
    char                recvline[MAXLINE + 1], sendline[MAXLINE];
    int                 maxfdp1;
    fd_set              rset;
	// pid_t				childpid;
	socklen_t			clilen;
	struct sockaddr_in  tcpaddr, cliaddr;
    struct timeval timeout;
	// void				sig_chld(int);

	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&tcpaddr, sizeof(tcpaddr));
	tcpaddr.sin_family      = AF_INET;
	tcpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpaddr.sin_port        = htons(SERV_PORT);


	Bind(listenfd, (SA *) &tcpaddr, sizeof(tcpaddr));

    Listen(listenfd, LISTENQ);

	// Signal(SIGCHLD, sig_chld);	/* must call waitpid() */

	// initial
    for(int i = 0; i < MAXPLAYER; i++) constent[i] = i;
    for(int i = 0; i < MAXPLAYER; i++)
    {
        conns[i].state = 0;

        // 初始化room data
        room_data[i].in_game = 0;
        room_data[i].new_player = 0;
        room_data[i].player_num = 0;
        room_data[i].random = 0;
        room_data[i].used = 0;
        for(int j = 0; j < 10; j++)
        {
            room_data[i].players[j] = -1;
        }

        // 一次開好lock
        sem_init(&room_data_lock[i], 0, 1);

        // 一次處理好thread pool
        room_working[i] = 0;
        handler_working[i] = 0;
        pthread_create(&handler[i], NULL, GetInRoom, &constent[i]);
        pthread_create(&handler[i], NULL, RoomProcess, &constent[i]);

    }
    global_player_num = 0;
    room_num = 0;
    used_player_list.next = &used_player_list;
    used_player_list.pre = &used_player_list;

    while(1)
    {
        __sync_synchronize();
        // initail select
        maxfdp1 = 0;
        FD_ZERO(&rset);
        if(global_player_num < MAXPLAYER)
        {
            FD_SET(listenfd, &rset);
            if(maxfdp1 < (listenfd + 1)) maxfdp1 = listenfd + 1;
        }
        for(struct link_node *temp = used_player_list.next; temp != &used_player_list; )
        {
            if(conns[temp->index].state == 1)
            {
                FD_SET(conns[temp->index].connfd, &rset);
                maxfdp1 = max(maxfdp1, conns[temp->index].connfd + 1);
                temp = temp->next;
            }
            else if(conns[temp->index].state == 0)  // state = 2 還是要留下來的
            {
                temp->next->pre = temp->pre;  // state = 0 把他踢出 used list
                temp->pre->next = temp->next;
                struct link_node *temp2;
                temp2 = temp;
                temp = temp->next;
                free(temp2);
            }
            else
            {
                temp = temp->next;
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 100;
        if(select(maxfdp1, &rset, NULL, NULL ,&timeout) < 0)
        {
            if (errno == EINTR)
                continue;		/* back to for() */
            else
                err_sys("select error");
        }
        // sprintf(buffer, "selecting...\n");
        // Writen(0, buffer, strlen(buffer));

        // select 到東西
        // listen
        if(FD_ISSET(listenfd, &rset))
        {
            int index = FindEmpty();
            if(index < 0) continue;
            if((conns[index].connfd = accept(listenfd, (SA *) &cliaddr, &clilen)) < 0)
            {
			    if (errno == EINTR)
                    continue;		/* back to for() */
                else
                    err_sys("accept error");
            }

            conns[index].state = 1;
            struct link_node *temp = malloc(sizeof(used_player_list));  // 新的塞進used list

            temp->index = index;
            used_player_list.pre->next = temp;
            temp->pre = used_player_list.pre;
            temp->next = &used_player_list;
            used_player_list.pre = temp;

            global_player_num++;
        }

        // 有東西read
        for(struct link_node *temp = used_player_list.next; temp != &used_player_list; )
        {
            // sprintf(buffer, "in main for\n");
            // Writen(0, buffer, strlen(buffer));
            // if(conns[temp->index].state == 2) continue;
            if(conns[temp->index].state == 1)
            {
                if(FD_ISSET(conns[temp->index].connfd, &rset))
                {
                    n = readline(conns[temp->index].connfd, recvline, MAXLINE);
                    if(n == 0)  // 他離開了
                    {
                        conns[temp->index].state = 0;
                        global_player_num--;
                        close(conns[temp->index].connfd);
                        continue;
                    }

                    sprintf(buffer, "recv from %d: %s", temp->index, recvline);
                    Writen(0, buffer, strlen(buffer));

                    char copy_recvline[MAXLINE];
                    strcpy(copy_recvline, recvline);
                    char *recv = strtok(copy_recvline, "\n");  // 去掉尾巴的換行
                    char *op = strtok(recv, " ");

                    if(strcmp(op, "1") == 0)
                    {
                        sprintf(sendline, "1\n");  // 告訴cli 尚未登入
                        Writen(conns[temp->index].connfd, sendline, strlen(sendline));
                    }

                    else if(op[0] < '0' || op[0] > '9')  // 第一個字不是數字->token
                    {
                        sprintf(conns[temp->index].name, op, strlen(op));
                        char *opop = strtok(NULL, " ");
                        if(strcmp(opop, "1") == 0)
                        {
                            if(AvailableToken(op)) sprintf(sendline, "4 %s\n", op);
                            else sprintf(sendline, "1\n");
                            Writen(conns[temp->index].connfd, sendline, strlen(sendline));
                        }
                    }

                    else if(strcmp(op, "2") == 0)
                    {
                        char *login = strtok(NULL, " ");
                        char *id = strtok(NULL, " ");
                        // char *password = strtok(NULL, " ");
                        // if(strcmp(login, "0") == 0)
                        // {
                        //     if(HandleSingnup(id)) 
                        //     {
                        //         sprintf(sendline, "2\n4 %s\n", id);
                        //         sprintf(conns[temp->index].name, "%s", id);
                        //     }
                        //     else sprintf(sendline, "3\n");
                        // }
                       if(strcmp(login , "1") == 0)
                        {
                            if(HandleLogin(id))
                            {
                                sprintf(sendline, "2\n4 %s\n", id);
                                sprintf(conns[temp->index].name, "%s", id);
                            }                            
                            else sprintf(sendline, "3\n");
                        }
                        else
                        {
                            sprintf(sendline, "3\n");
                        }
                        Writen(conns[temp->index].connfd, sendline, strlen(sendline));
                    }

                    else if(strcmp(op, "3") == 0)  // 開房間
                    {
                        handler_arg[temp->index].owner = temp->index;
                        handler_arg[temp->index].room_no = -1;
                        handler_arg[temp->index].random = 0;
                        conns[temp->index].state = 2;
                        handler_working[temp->index] = 1;
                    }

                    else if(strcmp(op, "4") == 0)  // 進房間
                    {
                        char *str_room_no = strtok(NULL, " ");
                        int room_no = atoi(str_room_no);
                        
                        // 可以正常加入房間了！
                        // struct room_thread_args args;
                        handler_arg[temp->index].owner = temp->index;
                        handler_arg[temp->index].room_no = room_no;
                        handler_arg[temp->index].random = 0;
                        conns[temp->index].state = 2;
                        handler_working[temp->index] = 1;

                        // pthread_create(&handler[temp->index], NULL, GetInRoom, &args);

                        // parent process
                        // close(conns[temp->index].connfd);

                    }

                    else if(strcmp(op, "5") == 0)  // 隨機
                    {
                        handler_arg[temp->index].owner = temp->index;
                        handler_arg[temp->index].room_no = -1;
                        handler_arg[temp->index].random = 1;
                        conns[temp->index].state = 2;
                        handler_working[temp->index] = 1;
                    }
                }
            }
            temp = temp->next;
        }
    }
}
