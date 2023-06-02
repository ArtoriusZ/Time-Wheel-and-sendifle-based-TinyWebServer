#include "lst_timer.h"
#include "tw_timer.h"
#include <sys/time.h>

const int TIMESLOT=5;
const int LOOP=10;

int main(int argc, char *argv[])
{
    time_wheel tw;
    sort_timer_lst lst;

    //假设50000个client分5批访问webserver，每批包含10000人且并发地进行访问, 每批访问间隔为1个TIMESLOT
    //这些client都请求长连接，且均为活跃连接，这意味着这些连接不会被close，其timer将一直存在。
    //测试上述场景下，两种不同定时器容器的添加定时器效率。

    struct timeval sTime_tw, eTime_tw;
    double total_time_tw=0;
    for(int loop=0;loop<LOOP;loop++)
    {
        gettimeofday(&sTime_tw,NULL);
        for(int i=0;i<5;i++)
        {
            for(int j=0;j<1000;j++)
            {
                tw.add_timer((i+1)*TIMESLOT);
            }
        }
        gettimeofday(&eTime_tw,NULL);
        total_time_tw+=(eTime_tw.tv_sec-sTime_tw.tv_sec)*1000000+(eTime_tw.tv_usec-sTime_tw.tv_usec);
    }
    cout<<total_time_tw/LOOP<<endl;

    struct timeval sTime_lst, eTime_lst;
    double total_time_lst=0;
    for(int loop=0;loop<LOOP;loop++)
    {
        gettimeofday(&sTime_lst,NULL);
        for(int i=0;i<5;i++)
        {
            for(int j=0;j<1000;j++)
            {
                time_t cur=time(NULL);
                util_timer *timer=new util_timer();
                timer->expire=cur+(i+1)*TIMESLOT;
                lst.add_timer(timer);
            }
        }
        gettimeofday(&eTime_lst,NULL);
        total_time_lst+=(eTime_lst.tv_sec-sTime_lst.tv_sec)*1000000+(eTime_lst.tv_usec-sTime_lst.tv_usec);
    }
    cout<<total_time_lst/LOOP<<endl;
}