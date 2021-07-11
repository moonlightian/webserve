#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool//一个线程池类
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量，connPool数据库连接指针*/
    threadpool(connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request);//向请求队列中插入任务请求

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之，如果处理线程函数为类成员函数时，需要将其设置为静态成员函数，无this指针，与类型匹配。*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    bool m_stop;                //是否结束线程
    connection_pool *m_connPool;  //数据库
};
template <typename T>//本类的构造函数
threadpool<T>::threadpool( connection_pool *connPool, int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];//创建描述线程池的数组，
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        //printf("create the %dth thread\n",i);
       //     int pthread_create (pthread_t *thread_tid,   //新创建的线程ID指向的内存单元。
      //     const pthread_attr_t *attr,         //指向线程属性的指针,通常设置为NULL,默认非分离属性的，
      //    void * (*start_routine) (void *),   //处理线程函数的地址，新创建的线程从函数的地址开始运行,const
     //     void *arg);                         //start_routine()中的参数
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))//将线程进行分离后，不用单独对工作线程进行回收
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
template <typename T>//析构函数
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}
template <typename T> //向请求队列中插入任务请求
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();//保护请求队列的互斥锁    
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();//通过信号量提醒有任务要处理
    return true;
}
template <typename T>//所有工作线程都要运行的函数  //它不断从工作队列中取出任务并执行之
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();//线程池类的对象作为参数（this）传递给静态函数(worker),在静态函数中引用这个对象,并调用其动态方法(run)。
    return pool;
}
template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_queuestat.wait();//信号量等待，直到收到了post来的信号
        m_queuelocker.lock();//被唤醒后先加互斥锁
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;

        connectionRAII mysqlcon(&request->mysql, m_connPool);//将request去处理
        
        request->process();
    }
}
#endif
