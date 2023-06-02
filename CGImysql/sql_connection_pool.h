#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool
{
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式 这个是利用局部静态变量懒汉模式实现单例（多线程不友好）
	//对于多线程而言，多个线程可能同时访问到该静态变量，并发现其没有被初始化（C++实现机制是）
	//（该看静态变量内部一标志位是为1，为1则说明已被初始化）
	//多个线程同时判定其没有初始化而后均初始化就会造成错误（即它不是一个原子操作）
	//PS-C++11之后局部静态变量线程安全
	static connection_pool *GetInstance();

	void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 

private:
	connection_pool();
	~connection_pool();

	int m_MaxConn;  //最大连接数
	int m_CurConn;  //当前已使用的连接数
	int m_FreeConn; //当前空闲的连接数
	locker lock;
	list<MYSQL *> connList; //连接池
	sem reserve; //用于多线程同步的信号量，其值表示空闲的连接个数

public:
	string m_url;			 //主机地址
	string m_Port;		 //数据库端口号
	string m_User;		 //登陆数据库用户名
	string m_PassWord;	 //登陆数据库密码
	string m_DatabaseName; //使用数据库名
	int m_close_log;	//日志开关
};


/*
这里是进行RAII机制封装。
具体为：当需要从连接池中取出一个连接时，可以创建一个connectionRAII对象，此对象的构造函数将从连接池内取出一个连接，存在成员变量conRAII中，
当超出次对象作用域时，会调用析构函数，自动释放取出的连接。
优点：避免了手动释放连接。
*/
class connectionRAII{

public:
	//这里con参数用二级指针，因为需要对一级指针进行修改。
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif
