#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

//用c++简单封装互斥锁
class Lock
{
public:

    void init_lock();
    void lock_t();
    void unlock_t();
    //void trylock_t();
private:
    pthread_mutex_t mutex;
};

void Lock::init_lock()
{
    pthread_mutex_init(&mutex, NULL); //初始化互斥锁
}
void Lock::lock_t()
{
    pthread_mutex_lock(&mutex);
}

void Lock::unlock_t()
{
    pthread_mutex_unlock(&mutex);
}

//void Lock::trylock_t()
//{
//    pthread_mutex_trylock(&mutex);
//}

//全局变量
int a = 0;
//创建对象
Lock lock;

//线程回调函数
void *thread1(void *arg)
{
    while(1)
    {
        printf("I am Thread 111.\n");
        lock.lock_t();
        a++;
        printf("thread 1  a == %d .\n", a);
        lock.unlock_t();
        sleep(1);
    }
}

void *thread2(void *arg)
{
    while(1)
    {
        printf("I am Thread 222.\n");
        lock.lock_t();
        a++;
        printf("thread 2  a == %d .\n", a);
        lock.unlock_t();
        sleep(2);
    }
}

int main()
{
    //创建两个线程
    pthread_t test1;
    pthread_t test2;

    lock.init_lock();

    pthread_create(&test1, NULL, thread1, NULL);
    pthread_create(&test2, NULL, thread2, NULL);

    pthread_join(test1, NULL);
    pthread_join(test2, NULL);
}
