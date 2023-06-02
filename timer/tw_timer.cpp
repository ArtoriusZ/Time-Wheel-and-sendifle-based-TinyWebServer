#include "../http/http_conn.h"
#include "tw_timer.h"


time_wheel::time_wheel(): cur_slot( 0 )
{
    for( int i = 0; i < N; ++i )
    {
            slots[i] = NULL;
    }
}

time_wheel::~time_wheel()
{
    //循環釋放每個槽內的所有定時器
    for( int i = 0; i < N; ++i )
    {
        tw_timer* tmp = slots[i];
        while( tmp )
        {
            slots[i] = tmp->next;
            delete tmp;
            tmp = slots[i];
        }
    }
}

//创建一个定时器，并将其添加到时间轮中。
tw_timer* time_wheel::add_timer( int timeout )
{
    //非法输入，返回null
    if( timeout < 0 )
    {
        return NULL;
    }

    //计算超时时间对应的tick数
    //不足TI则向上折为1个tick，反之向下折为timeout/TI个tick
    int ticks = 0;
    if( timeout < TI )
    {
        ticks = 1;
    }
    else
    {
        ticks = timeout / TI;
    }

    //根据tick数计算rotation以及新的定时器应在的槽位置
    int rotation = ticks / N;
    int ts = ( cur_slot + ( ticks % N ) ) % N;//ts为槽位置

    //创建新的定时器，并将其加到时间轮的对应槽内
    tw_timer* timer = new tw_timer( rotation, ts );
    if( !slots[ts] ) //如果对应槽内为空，直接插入
    {
        //printf( "add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot );
        slots[ts] = timer;
    }
    else //如果对应槽内不空，用头插法插入
    {
        timer->next = slots[ts];
        slots[ts]->prev = timer;
        slots[ts] = timer;
    }

    return timer;
}

//从时间轮上删除一个定时器。
void time_wheel::del_timer( tw_timer* timer )
{
    //处理非法输入
    if( !timer )
    {
        return;
    }
    
    //进行删除
    int ts = timer->time_slot;
    if( timer == slots[ts] ) //如果是该槽内第一个定时器
    {
        slots[ts] = slots[ts]->next;
        if( slots[ts] )
        {
            slots[ts]->prev = NULL;
        }
        delete timer;
    }
    else //如果不是第一个
    {
        timer->prev->next = timer->next;
        if( timer->next )
        {
            timer->next->prev = timer->prev;
        }
        delete timer;
    }
}

//调整定时器的位置，这里只考虑任务被延长的情况
void time_wheel::adjust_timer(tw_timer *timer, int timeout)
{
    //处理非法输入
    if(!timer) return;

    //从时间轮上取出该定时器，注意不是删除
    int old_ts = timer->time_slot;
    if( timer == slots[old_ts] ) //如果是该槽内第一个定时器
    {
        slots[old_ts] = slots[old_ts]->next;
        if( slots[old_ts] )
        {
            slots[old_ts]->prev = NULL;
        }
    }
    else //如果不是第一个
    {
        timer->prev->next = timer->next;
        if( timer->next )
        {
            timer->next->prev = timer->prev;
        }
    }
    timer->prev=nullptr; //这里要重置前后指针
    timer->next=nullptr;

    //计算该定时器的新位置
    int ticks = 0;
    if( timeout < TI )
    {
        ticks = 1;
    }
    else
    {
        ticks = timeout / TI;
    }
    timer->rotation = ticks / N;
    timer->time_slot = ( cur_slot + ( ticks % N ) ) % N;
    
    //将该定时器重新插入时间轮内
    int new_ts=timer->time_slot;
    if( !slots[new_ts] ) //如果对应槽内为空，直接插入
    {
        //printf( "add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot );
        slots[new_ts] = timer;
    }
    else //如果对应槽内不空，用头插法插入
    {
        timer->next = slots[new_ts];
        slots[new_ts]->prev = timer;
        slots[new_ts] = timer;
    }
}

//心博函数
void time_wheel::tick()
{
    tw_timer* tmp = slots[cur_slot];
    //printf( "current slot is %d\n", cur_slot );

    //遍历当前时间槽内的定时器
    while( tmp )
    {
        //如果rotation>0，则此定时器在当前轮不生效，更新其rotation
        if( tmp->rotation > 0 )
        {
            tmp->rotation--;
            tmp = tmp->next;
        }
        //反之则生效，执行其定时任务并删除
        else
        {
            tmp->cb_func( tmp->user_data );
            if( tmp == slots[cur_slot] )
            {
                //printf( "delete header in cur_slot\n" );
                slots[cur_slot] = tmp->next;
                delete tmp;
                if( slots[cur_slot] )
                {
                    slots[cur_slot]->prev = NULL;
                }
                tmp = slots[cur_slot];
            }
            else
            {
                tmp->prev->next = tmp->next;
                if( tmp->next )
                {
                    tmp->next->prev = tmp->prev;
                }
                tw_timer* tmp2 = tmp->next;
                delete tmp;
                tmp = tmp2;
            }
        }
    }
    //转到下一个槽
    cur_slot = ++cur_slot % N;
}


//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    //fcntl可以获取/设置文件描述符性质
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，是否选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    //如果对描述符socket注册了EPOLLONESHOT事件，
    //那么操作系统最多触发其上注册的一个可读、可写或者异常事件，且只触发一次。
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_wheel.tick();
    //重新设置定时器
    //最小的时间单位为5s
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;


class Utils;
//定时器回调函数:从内核事件表删除事件，关闭文件描述符，释放连接资源
void cb_func(client_data *user_data)
{
    //删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    //删除非活动连接在socket上的注册事件
    close(user_data->sockfd);
    //减少连接数
    http_conn::m_user_count--;
}