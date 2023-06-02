#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //时间轮槽间隔

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port , string user, string passWord, string databaseName,
              int log_write , int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model,int zero_copy);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(tw_timer *timer);
    void deal_timer(tw_timer *timer, int sockfd);
    bool dealclinetdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    //基础信息
    int m_port;//端口
    char *m_root;//根目录
    int m_log_write;//日志类型
    int m_close_log;//是否启动日志
    int m_actormodel;//Reactor/Proactor
    int m_zerocopy; //零拷贝方式
    //网络信息
    int m_pipefd[2];//此管道用于统一信号事件源，即信号抵达时，通过此管道向主线程传递消息。
    int m_epollfd;//epoll对象

    http_conn *users;//HTTP连接数组

    //数据库相关
    connection_pool *m_connPool;
    string m_user;         //登陆数据库用户名
    string m_passWord;     //登陆数据库密码
    string m_databaseName; //使用数据库名
    int m_sql_num;//数据库连接池数量

    //线程池相关
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;//监听套接字
    int m_OPT_LINGER;//是否优雅下线
    int m_TRIGMode;//ET/LT
    int m_LISTENTrigmode;//ET/LT
    int m_CONNTrigmode;//ET/LT

    //定时器相关
    client_data *users_timer;
    //工具类
    Utils utils;
};
#endif
