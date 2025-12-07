#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>

/* const numbers define */
#define ROW 17
#define COLUMN 49
#define HORI_LINE '-'
#define VERT_LINE '|'
#define CORNER '+'
#define PLAYER '0'
#define WALL '='
#define GOLD_SHARDS '$'
#define BG_UPDATE_TIME 100000
#define RESULT_WAIT_TIME 200000

struct GoldShard {
    int thread_id;
    int row;
    int col;
    bool direction;
    bool on_screen;
};

/* global variables */
int player_x;
int player_y;
char map[ROW][COLUMN + 1];
GoldShard* goldShards[6];
bool finished;
int points;

/* functions */
int kbhit(void);
void map_print(void);
void check_player_move_result(int x, int y);

/* Determine a keyboard is hit or not.
 * If yes, return 1. If not, return 0. */
int kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

/* print the map */
void map_print(void)
{
    printf("\033[H\033[2J");
    int i;
    for (i = 0; i <= ROW - 1; i++)
        puts(map[i]);
}

/* create each wall */
void *create_wall(void* arg)
{
    int threadid = (int)(intptr_t)arg;

    // which row
    int row_number;
    if(threadid < 3){
        row_number = 2 * (threadid + 1);
    }else{
        row_number = 2 * (threadid + 2);
    }

    // create the wall
    int begin_col = rand() % 47;
    for(int i = begin_col; i < begin_col + 15; i++)
    {
        map[row_number][i % 47 + 1] = WALL;
    }

    pthread_exit(NULL);
}

/* create each gold shard */
void *create_shard(void* arg)
{
    int threadid = (int)(intptr_t)arg;
    GoldShard* goldShard = (GoldShard*)malloc(sizeof(GoldShard));
    goldShard->thread_id = threadid;

    // which row
    int row_number;
    if(threadid < 3){
        row_number = 2 * threadid + 1;
    }else{
        row_number = 2 * (threadid + 2) + 1;
    }

    // create the gold shard
    int col = rand() % 47;
    map[row_number][col + 1] = GOLD_SHARDS;

    goldShard->row = row_number;
    goldShard->col = col + 1;
    goldShard->on_screen = true;

    int dir = rand() % 2;
    if(dir == 0)
    {
        goldShard->direction = true;
    }else
    {
        goldShard->direction = false;
    }
    goldShards[threadid] = goldShard;
    pthread_exit(goldShard);
}

/* move each wall */
void *move_wall(void* arg)
{
    int threadid = (int)(intptr_t)arg;

    // which row
    int row_number;
    if(threadid < 3){
        row_number = 2 * (threadid + 1);
    }else{
        row_number = 2 * (threadid + 2);
    }

    //find the begin column
    int begin_col;
    for(int i = 1 ; i < 48; i ++)
    {
        if(map[row_number][i % 47 + 1] == WALL && map[row_number][(i - 1) % 47 + 1] == ' ')
        {
            begin_col = i;
            map[row_number][i % 47 + 1] = ' ';
            if(map[row_number][(i + 15) % 47 + 1] == PLAYER)
            {
                finished = true;
                usleep(RESULT_WAIT_TIME);
                printf("\033[H\033[2J");
                printf("You lose the game!!\n");
            }
            map[row_number][(i + 15) % 47 + 1] = WALL;
            break;
        }
    }

    pthread_exit(NULL);
}

/* move each gold shards */
void *move_shard(void* arg)
{
    GoldShard* gold = (GoldShard*)arg;

    if(!gold->on_screen)
    {
        pthread_exit(gold);
        return NULL;
    }

    int row = gold->row;
    int col = gold->col;
    map[row][col] = ' ';

    bool direction = gold->direction;
    if(direction)
    {
        col = (col - 1 + 1) % 47 + 1;
    }else
    {
        col = (col - 1 - 1 + 47) % 47 + 1;
    }

    //check if it is the player
    if(map[row][col] == PLAYER)
    {
        points ++;
        if(points >= 6)
        {
            finished = true;
            usleep(RESULT_WAIT_TIME);
            printf("\033[H\033[2J");
            printf("You win the game!!\n");
        }else
        {
            gold->on_screen = false;
            gold->col = col;
            pthread_exit(gold);
            return NULL;
        }
    }

    map[row][col] = GOLD_SHARDS;
    gold->col = col;
    pthread_exit(gold);
}

/* control the player */
void *control_player(void* arg)
{
    int x = ROW / 2;
    int y = COLUMN / 2;

    while (!finished)
    {
        if (kbhit())
        {
            char dir = getchar();
            if (dir == 'w' || dir == 'W')
            {
                map[x][y] = ' ';
                x = (x - 1 - 1 + 15) % 15 + 1;
                check_player_move_result(x, y); 
            }
            if (dir == 's' || dir == 'S')
            {
                map[x][y] = ' ';
                x = (x - 1 + 1) % 15 + 1;
                check_player_move_result(x, y); 
            }
            if (dir == 'a' || dir == 'A')
            {
                map[x][y] = ' ';
                y = (y - 1 - 1 + 47) % 47 + 1;
                check_player_move_result(x, y); 
            }
            if (dir == 'd' || dir == 'D')
            {
                map[x][y] = ' ';
                y = (y - 1 + 1) % 47 + 1;
                check_player_move_result(x, y); 
            }
            if (dir == 'q' || dir == 'Q')
            {
                finished = true;
                usleep(RESULT_WAIT_TIME);
                printf("\033[H\033[2J");
                printf("You exit the game.\n");
            }
        }
    }

    pthread_exit(NULL);
}

void check_player_move_result(int x, int y)
{
    char thisChar = map[x][y];
    if(thisChar == ' ')
    {
        map[x][y] = PLAYER;
        map_print();
        return;
    }
    if(thisChar == WALL)
    {
        finished = true;
        usleep(RESULT_WAIT_TIME);
        printf("\033[H\033[2J");
        printf("You lose the game!!\n");
        return;
    }
    if(thisChar == GOLD_SHARDS)
    {
        map[x][y] = PLAYER;
        points ++;
        if(points >= 6)
        {
            finished = true;
            usleep(RESULT_WAIT_TIME);
            printf("\033[H\033[2J");
            printf("You win the game!!\n");
        }else
        {
            for(GoldShard* gold : goldShards)
            {
                if(gold != NULL && gold->row == x && gold->col == y) 
                {
                    gold->on_screen = false;
                    break;
                }
            }
            map_print();
        }
        return;
    }
    return;
}

/* main function */
int main(int argc, char *argv[])
{
    srand(time(NULL));
    int i, j;
    points = 0;

    /* initialize the map */
    memset(map, 0, sizeof(map));
    for (i = 1; i <= ROW - 2; i++)
    {
        for (j = 1; j <= COLUMN - 2; j++)
        {
            map[i][j] = ' ';
        }
    }
    for (j = 1; j <= COLUMN - 2; j++)
    {
        map[0][j] = HORI_LINE;
        map[ROW - 1][j] = HORI_LINE;
    }
    for (i = 1; i <= ROW - 2; i++)
    {
        map[i][0] = VERT_LINE;
        map[i][COLUMN - 1] = VERT_LINE;
    }
    map[0][0] = CORNER;
    map[0][COLUMN - 1] = CORNER;
    map[ROW - 1][0] = CORNER;
    map[ROW - 1][COLUMN - 1] = CORNER;

    player_x = ROW / 2;
    player_y = COLUMN / 2;
    map[player_x][player_y] = PLAYER;

    finished = false;

    map_print();

    /* create the walls */
    int NUM_WALLS = 6;
    pthread_t creat_wall_threads[NUM_WALLS];
    int rc, t;
    for (t = 0; t < NUM_WALLS; t++)
    {
        rc = pthread_create(&creat_wall_threads[t], NULL, create_wall, (void *)t);
        if (rc)
        {
            printf("ERROR: return code from pthread_create() is %d\n", rc);
            exit(-1); 
        }
    }

    /* create the gold shards */
    int NUM_GOLD_SHARDS = 6;
    pthread_t creat_shard_threads[NUM_GOLD_SHARDS];
    for (t = 0; t < NUM_GOLD_SHARDS; t++)
    {
        srand(time(0));
        rc = pthread_create(&creat_shard_threads[t], NULL, create_shard, (void *)t);
        if (rc)
        {
            printf("ERROR: return code from pthread_create() is %d\n", rc);
            exit(-1); 
        }
    }
    for (t = 0; t < NUM_WALLS; t++)
    {
        pthread_join(creat_wall_threads[t], NULL);
    }
    for (t = 0; t < NUM_GOLD_SHARDS; t++)
    {
        GoldShard* goldShard;
        pthread_join(creat_shard_threads[t], (void**)&goldShard);
    }
    map_print();

   /* control the player movement */
    pthread_t control_player_thread;
    int rc_player;
    rc_player = pthread_create(&control_player_thread, NULL, control_player, NULL);
    if (rc_player)
    {
        printf("ERROR: return code from pthread_create() is %d\n", rc_player);
        exit(-1); 
    }

    /* game is processing */
    while(!finished)
    {
        usleep(BG_UPDATE_TIME);

        //move the walls
        int NUM_WALLS = 6;
        pthread_t move_wall_threads[NUM_WALLS];
        int rc, t;
        for (t = 0; t < NUM_WALLS; t++)
        {
            srand(time(0));
            rc = pthread_create(&move_wall_threads[t], NULL, move_wall, (void *)t);
            if (rc)
            {
                printf("ERROR: return code from pthread_create() is %d\n", rc);
                exit(-1); 
            }
        }

        //move the gold shards
        int NUM_GOLD_SHARDS = 6;
        pthread_t move_shard_threads[NUM_GOLD_SHARDS];
        for (t = 0; t < NUM_GOLD_SHARDS; t++)
        {
            if(goldShards[t] == NULL && !goldShards[t]->on_screen) continue;
            srand(time(0));
            rc = pthread_create(&move_shard_threads[t], NULL, move_shard, (void *)goldShards[t]);
            if (rc)
            {
                printf("ERROR: return code from pthread_create() is %d\n", rc);
                exit(-1); 
            }
        }
        for (t = 0; t < NUM_WALLS; t++)
        {
            pthread_join(move_wall_threads[t], NULL);
        }
        for (t = 0; t < NUM_GOLD_SHARDS; t++)
        {
            GoldShard* goldShard;
            pthread_join(move_shard_threads[t], (void**)&goldShard);
            goldShards[t] = goldShard;
        }
        if(!finished) map_print();
    }

    pthread_exit(NULL);

    return 0;
}
