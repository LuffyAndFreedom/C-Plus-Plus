/*************************************************************************
	> File Name: sever.cpp
	> Author: 吕白
	> Purpose:
	> Created Time: 2018年02月05日 星期一 16时47分37秒
 ************************************************************************/

#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<sys/types.h>
#include<signal.h>
#include<iostream>
#include<unistd.h>
#include<assert.h>
#include<fcntl.h>
#include<errno.h>
#include<cstdlib>
#include<cstdio>
#include<cstring>
#include<time.h>
#include<string>
#include<vector>
#include<queue>
#include<list>
using namespace std;

const int MAxBuf = 100;
const int MinUsual = 5;
const int MaxUsual = 10;
const int PORT = 4507;
const char *ip = "127.0.0.1";
const int sock_count = 256;

//任务的基类
class Job 
{
protected:
    int sockfd;     
    int Number;     //任务选项
    char *buf;      //缓冲区
public:
    Job();
    ~Job();
    int GetSock();
    char *GetBuf();
    virtual void Run( int sock, char *buffer ) = 0;
    
};

//实际的线程池
class ThreadPool 
{

protected:
    static void *FunThread( void *arg );    //线程的调度函数
private:
    static bool shutdown;   //标志着线程的退出
    static vector <Job *> JobList;  //任务列表
    static pthread_mutex_t mutex;
    static pthread_cond_t cond;

    //pthread_t *pid;
    static vector < pthread_t > free;   //闲置线程
    static vector < pthread_t > busy;   //忙碌线程

    int InitNum;    //初始化线程数
    static int NowNum;     //现在拥有的空闲线程数
    int UsualMin;   //允许保持的最少的线程数
    int UsualMax;   //允许保持的最大空闲线程数
public:
    ThreadPool();
    ~ThreadPool();
    void create();   //创建线程池中的线程
    void StopAll();  //销毁线程池
    void AddJob( Job *job );   //添加任务
    void IncThread();   //向线程池中添加线程
    void RedThread();   //减少线程池中的线程
    int Getsize();  //获取当前任务队列中的任务
};

/*
//线程池的接口　管理线程池
class ThreadManage 
{
    
private:
    ThreadPool *pool;   //指向实际的线程池
public:
    ThreadManage();
    ~ThreadManage();
};
*/
Job :: Job()
{
    Number = -1;
    buf = new char[ MAxBuf ];
}

Job :: ~Job()
{
    Number = -1;
    delete[] buf;
    buf = nullptr;
}

int Job :: GetSock() 
{
    return sockfd;
}

char *Job :: GetBuf() 
{
    return buf;
}

/*
//线程池的接口
ThreadManage :: ThreadManage() 
{
    pool = new ThreadPool;
}

ThreadManage :: ~ThreadManage() 
{
    delete pool;
    pool = nullptr;
}
*/


//初始化静态变量
bool ThreadPool :: shutdown = false;
vector <Job *> ThreadPool :: JobList;
pthread_mutex_t ThreadPool :: mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ThreadPool :: cond = PTHREAD_COND_INITIALIZER;
vector < pthread_t > ThreadPool :: free;
vector < pthread_t > ThreadPool :: busy;
int ThreadPool :: NowNum;

ThreadPool :: ThreadPool()
{
    InitNum = 5;
    NowNum = InitNum;
    UsualMax = MaxUsual;
    UsualMin = MinUsual;
    //pid = new pthread_t[ InitNum ];
    free.resize( InitNum ); 
    busy.resize( InitNum );

    create();
}

ThreadPool :: ~ThreadPool()
{

}

void ThreadPool :: create()
{
    for ( int i = 0; i < InitNum; i++ ) 
    {
        pthread_create( &free[i], nullptr, FunThread, nullptr );
    }
}

void ThreadPool :: AddJob( Job *job ) 
{
    pthread_mutex_lock( &mutex );
    JobList.push_back( job );
    IncThread();    //检测是否需要添加线程
    RedThread();    //检测是否需要回收线程
    pthread_cond_signal( &cond );   //任务添加后发出信号
    pthread_mutex_unlock( &mutex );

}

void ThreadPool :: StopAll() 
{
    if ( shutdown ) //避免重复调用
    {
        return ;
    }
    shutdown = true;
    pthread_cond_broadcast( &cond );    //唤醒所有线程
    for ( int i = 0; i < NowNum; i++ ) 
    {
        pthread_join( free[i], nullptr );
    }
    //delete[] pid;
    //pid = nullptr;

    pthread_mutex_destroy( &mutex );
    pthread_cond_destroy( &cond );
}

void* ThreadPool :: FunThread( void *arg ) 
{
    pthread_t tid = pthread_self();
    while (1) 
    {
        pthread_mutex_lock( &mutex );
        //如果没有任务且仍在正常运行　等待
        while ( !shutdown && JobList.size() == 0 ) 
        {
            pthread_cond_wait( &cond, &mutex );     //等待添加任务的唤醒
        }

        //退出线程
        if ( shutdown ) 
        {
            pthread_mutex_unlock( &mutex );
            cout << tid << " exit\n";
            pthread_exit( nullptr );
        }
        
        //取出一个任务
        vector <Job *> :: iterator itr = JobList.begin();
        Job *job = *itr;
        JobList.erase( itr ); //从任务队列删除取出的任务
        NowNum--;   //空闲线程减一
        
        vector < pthread_t > :: iterator iter, end;     //将该线程从闲置移到忙碌
        for ( iter = free.begin(), end = free.end(); iter != end; iter++ ) 
        {
            if ( *iter == tid ) 
            {
                break;
            }
        }
        busy.push_back( tid );
        free.erase( iter );

        pthread_mutex_unlock( &mutex );
        job->Run( job->GetSock(), job->GetBuf() );  //执行任务
        NowNum++; // 空闲线程加一
        
        for ( iter = busy.begin(), end = busy.end(); iter != end; iter++ )  //将该线程从忙碌移到闲置 
        {
            if ( *iter == tid ) 
            {
                break;
            }
        }
        free.push_back( tid );
        busy.erase( iter );
    }
}

int ThreadPool :: Getsize()  
{
    return JobList.size();
}

void ThreadPool :: IncThread()  //添加线程 
{
    int JobLength = Getsize();
    //如果有大于1个任务都在等待 添加线程
    if ( (JobLength - NowNum) >= 1 )  
    {
        pthread_t tmp;
        free.push_back( tmp ); //添加到闲置线程队列中
        pthread_create( &tmp, nullptr, FunThread, nullptr );
    }
}

void ThreadPool :: RedThread()      //减少线程
{
    if ( free.size() >= 8 )     //如果闲置线程超过8个 
    {
        pthread_mutex_lock( &mutex );
        for ( int i = 0; free.size() >= UsualMin; i++ )     //回收到不超过Usualmin个
        {
            pthread_cond_signal( &cond );
            pthread_join( free[i], nullptr );
        }
        pthread_mutex_unlock( &mutex );
    }
}

class MyJob : public Job    //实际的任务类 
{
public:
    void Run( int sockfd, char *buf );
    void Set( int num, char *buf, int sock );
    void Randstr( char *buffer );
    void GetTime( char *buffer );
    void Time( char *buffer );
};

void MyJob :: Set( int num, char *buffer, int sock )     //设置数据
{
    sockfd = sock;
    Number = num;
    strcpy( buf,buffer );
}

void MyJob :: Randstr( char *buffer )   //产生随机字符串
{
    int randnum;
    char str[63] = "qwertyuioplkjhgfdsazxcvbnmQWERTYUIOPLKJHGFDSAZXCVBNM0123456789";
    for ( int i = 0; i < 10; i++ ) 
    {
        randnum = rand()%62;    //产生随机数
        *buffer = str[randnum];
        buffer++;
    }
    *buffer = '\0';
}

void MyJob :: GetTime( char *buffer )     //获取时间
{
    bzero( buffer, sizeof( buffer ) );
    time_t t;
    tm *local;
    t = time( nullptr );
    local = localtime( &t );
    strftime( buffer, 64, "%Y-%m-%d %H:%M:%S", local ); 
}

void MyJob :: Time( char *buffer )  //以二进制发送从epoch到现在的秒数 
{
    time_t t;
    t = time( nullptr );
    char tmp[256];
    sprintf( tmp, "%d", (int)t );   //第三个参数期待int类型
    int change, k = 0, mask = 256;
    char bit;
    for ( short i = 0; i < strlen(tmp); i++ ) 
    {
        for ( int j = 0; j < 9; j++ ) 
        {
            buffer[k++] = ( mask & (tmp[i]/*-48*/) ) ? 49 : 48; //直接拿ACSLL码来用
            mask >>= 1;
        }
        mask = 256;
    }
}

void MyJob :: Run( int sock, char *buffer )     //执行任务
{
    if ( Number == 1 )  //echo 回显服务
    {
        send( sock, (void *)buffer, 256, 0 );
    }
    else if ( Number == 2 )     //discard 丢弃所有数据
    {
       bzero( buf, sizeof(buffer) ); 
    }
    else if ( Number == 3 )     //chargen 不停发送测试数据
    {
        Randstr( buffer );
        while ( (send( sock, (void *)buffer, 256, 0 ) ) != -1 ) 
        {
            sleep( 1 );
            Randstr( buffer );
        }
    }
    else if ( Number == 4 )     //daytime 以字符串形式发送当前时间
    {
        GetTime( buffer );
        send( sock, (void *)buffer, 256, 0 );
    }
    else if ( Number == 5 )     //time 以二进制形式发送当前时间
    {
        Time( buffer );
        send( sock, (void *)buffer, 256, 0 );
    }
    bzero( buffer, sizeof( buffer ) );
}


//封装epoll类
class MyEpoll 
{
public:
    MyEpoll();
    ~MyEpoll();
    void Init();    //初始化
    int Epoll_wait();   //等待
    void Epoll_new_client();     //新客户端
    void Epoll_recv( int sockfd );   //接收数据
    void Epoll_send( int sockfd );   //发送数据
    void Epoll_close( int sockfd );     //断开连接
    void Epoll_run();   //运行
private:
    int sock;
    int epfd;
    ThreadPool *pool;
    MyJob task;
    struct epoll_event ev, *event;  //ev用于处理事件　event数组用于回传要处理的事件
};

MyEpoll :: MyEpoll() 
{
    sock = 0;
    epfd = 0;
    event = new epoll_event[20];
    pool = new ThreadPool;
}

MyEpoll :: ~MyEpoll() 
{
    delete[] event;
    delete pool;
    pool = nullptr;
    event = nullptr;
}

void MyEpoll :: Init() 
{
    int ret;
    struct sockaddr_in address;
    bzero( &address, sizeof(address) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    //address.sin_addr.s_addr = htonl( INADDR_ANY );
    address.sin_port = htons( PORT );
   
    sock = socket( PF_INET, SOCK_STREAM, 0 );   
    assert( sock != -1 );
    
    ret = bind( sock, (struct sockaddr*)&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( sock, 5 );
    assert( ret != -1 );

    epfd = epoll_create( 1 );
    //epfd = epoll_create( sock_count );  //创建epoll句柄
    
    ev.data.fd = sock;  //设置与要处理的事件相关的文件描述符
    ev.events = EPOLLIN | EPOLLET;  //设置为ET模式
    epoll_ctl( epfd, EPOLL_CTL_ADD, sock, &ev );    //注册新事件
}

int MyEpoll :: Epoll_wait()     //等待事件 
{
    return epoll_wait( epfd, event, 20, -1 );   //-1表示一直等下去直到有时间发生 为任意正整数的时候表示等这么久
}

void MyEpoll :: Epoll_new_client() 
{
    sockaddr_in client_addr;
    bzero( &client_addr, sizeof(client_addr) );
    socklen_t clilen = sizeof( client_addr );
    
    int client = accept( sock, (struct sockaddr*)&client_addr, &clilen ); //接受连接
    
    ev.events = EPOLLIN | EPOLLET;  
    ev.data.fd = client;    
    epoll_ctl( epfd, EPOLL_CTL_ADD, client, &ev );  //注册新事件

}

struct tmp  //接收数据
{
    int number;
    char buf[256];
};
void MyEpoll :: Epoll_recv( int sockfd )
{
    tmp buf;//char buf[256];
    int nbytes;
    if ( nbytes = recv( sockfd, (void *)&buf, sizeof(buf), 0 ) <= 0 )  
    {
        ev.data.fd = sockfd;
        ev.events = EPOLLERR;   //对应文件描述符发生错误
        epoll_ctl( epfd, EPOLL_CTL_DEL, sockfd, &ev );
        Epoll_close( sockfd ); //断开连接
        return ;
    }
    task.Set( buf.number, buf.buf, sockfd );
    pool->AddJob( &task ); //往线程池添加任务
}


void MyEpoll :: Epoll_send( int sockfd ) 
{
    
}

void MyEpoll :: Epoll_close( int sockfd ) 
{
    close( sockfd );
}

void MyEpoll :: Epoll_run() 
{
    int nfds;
    for ( ; ; ) 
    {
        nfds = Epoll_wait();
        if ( nfds == 0 ) 
        {
            cout << "Time Out\n";
            continue;
        }
        else if ( nfds == -1 ) 
        {
            cout << "Error\n";
        }
        else 
        {
            for ( int i = 0; i < nfds; i++ )  //处理发生的所有事
            {
                if ( event[i].data.fd == sock )     //有新的客户端连接
                {
                    Epoll_new_client();
                }
                else 
                {
                    if ( event[i].events & EPOLLIN ) //有可读事件 
                    {
                        Epoll_recv( event[i].data.fd );
                    }
                    else if ( event[i].events & EPOLLOUT ) //有可写事件
                    {
                        Epoll_send( event[i].data.fd );
                    }
                }
            }
        }
        bzero( event, sizeof(event) );  //清空event数组
    }
}

int main()
{
    MyEpoll epoll;
    epoll.Init();
    epoll.Epoll_run();
}