#ifndef TW_TIMER
#define TW_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
#include "../log/log.h"


class tw_timer;

//连接资源
struct client_data
{
    sockaddr_in address;
    int sockfd;
    tw_timer* timer;
};

//定时器类
class tw_timer
{
public:
    tw_timer( int rot, int ts ) 
    : next( NULL ), prev( NULL ), rotation( rot ), time_slot( ts ){}

public:
    //记录时间轮转多少圈后此定时器生效
    int rotation;
    //记录此定时器对应的时间槽位置
    int time_slot;
    //回调函数
    void (*cb_func)( client_data* );
    //连接资源
    client_data* user_data;
    //指向前一个定时器
    tw_timer* next;
    //指向后一个定时器
    tw_timer* prev;
};

//时间轮类
class time_wheel
{
public:
    time_wheel();
    ~time_wheel();

    /*
    创建一个定时器，并将其添加到时间轮中。时间复杂度O(1)。
    @param timeout 添加的定时器的超时时间,注意这里是相对时间
    @return 新创建的定时器指针
    */
    tw_timer* add_timer( int timeout );

    /*
    从时间轮上删除一个定时器。时间复杂度O(1)。
    @param timer 被删除的定时器
    */
    void del_timer( tw_timer* timer );

    /*
    调整定时器的位置，这里只考虑任务被延长的情况
    @param timer 需要调整的定时器
    @param timeout 新的相对超时时间
    */
    void adjust_timer(tw_timer *timer,int timeout);

    /*
    心博函数
    */
    void tick();

private:
    //时间轮上槽的数目
    static const int N = 60;
    //槽间隔时间，单位s, 即每1s时间轮向后转动一个槽，也称为1个tick
    static const int TI = 5; 
    //时间轮
    tw_timer* slots[N];
    //当前槽
    int cur_slot;
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;//管道id
    time_wheel m_timer_wheel;//升序链表定时器
    static int u_epollfd;//epollfd
    int m_TIMESLOT=5;//最小时间间隙
};

void cb_func(client_data *user_data);

#endif