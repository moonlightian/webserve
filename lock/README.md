
线程同步机制包装类
===============
多线程同步：确保任一时刻只能有一个线程能进入关键代码段（临界区）. 临界区是指执行数据更新的代码需要独占式地执行。
> * 信号量 https://blog.csdn.net/u013457167/article/details/78318932
> * 互斥锁
> * 条件变量

# RAII
RAII全称是“Resource Acquisition is Initialization”，直译过来是“资源获取即初始化”.  
在构造函数中申请分配资源，在析构函数中释放资源。因为C++的语言机制保证了，当一个对象创建的时候，自动调用构造函数，当对象超出作用域的时候会自动调用析构函数。所以，在RAII的指导下，我们应该使用类来管理资源，将资源和对象的生命周期绑定,实现资源和状态的安全管理,智能指针是RAII最好的例子.  

# 信号量
信号量一般常用于保护一段代码，使其每次只被一个执行线程运行  
```cpp
int sem_init(sem_t *sem,int pshared,unsigned int value);
int sem_wait(sem_t *sem); 
int sem_post(sem_t *sem); 
int sem_destroy(sem_t *sem); 
```
sem_init用于对指定信号初始化，pshared为0，表示信号在当前进程的多个线程之间共享，否则信号量就可以在多个进程之间共享,value表示初始化信号的值。  
sem_wait表示等待这个信号量，直到信号量的值大于0，解除阻塞。解除阻塞后，sem的值-1，表示公共资源被执行减少了。当初始化value=0后，使用sem_wait会阻塞这个线程，这个线程函数就会等待其它线程函数调用sem_post增加了了这个值使它不再是0，才开始执行,然后value值-1。  
sem_post用于增加信号量的值+1，当有线程阻塞在这个信号量上时，调用这个函数会使其中的一个线程不在阻塞，选择机制由线程的调度策略决定。  

以上，成功返回0，失败返回errno,-1

# 互斥量/互斥锁
```cpp
int pthread_mutex_init(&m_mutex, NULL);
int pthread_mutex_destroy(&m_mutex);
int pthread_mutex_lock(&m_mutex);
int pthread_mutex_unlock(&m_mutex);
```
pthread_mutex_init函数以动态方式创建互斥锁，参数attr指定了新建互斥锁的属性。如果参数attr为NULL，则使用默认的互斥锁属性，默认属性为快速互斥锁.  
pthread_mutex_destory函数用于销毁互斥锁  
pthread_mutex_lock函数以原子操作方式给互斥锁加锁  
pthread_mutex_unlock函数以原子操作方式给互斥锁解锁  

以上，成功返回0，失败返回errno,-1

# 条件变量
信号量、共享内存，以及消息队列等主要关注**进程间**通信；而条件变量、互斥锁，主要关注**线程间**通信。

条件变量提供了一种线程间的通知机制,当某个共享数据达到某个值时,**唤醒**等待这个共享数据的线程.  
一个线程用于修改这个变量使其满足其它线程继续往下执行的条件，其它线程则**接收**条件已经发生改变的信号。  
条件变量同锁一起使用使得线程可以以一种**无竞争**的方式等待任意条件的发生。所谓无竞争就是，条件改变这个信号会发送到所有等待这个信号的线程。而不是说一个线程接受到这个消息而其它线程就接收不到了.  
http://blog.chinaunix.net/uid-27164517-id-3282242.html
https://www.cnblogs.com/harlanc/p/8596211.html
假设生产者消费者模型中：
```cpp
void thread_product(){//生产者线程
       lock();//加锁开始生产
       produce();//生产
       unlock();//生产完了，解锁
       signal();//去通知消费者线程
 }


void thread_consumer(){//消费者线程
   lock();//先加锁,这锁用来保护共享变量的，因为有多个消费者线程，对共享变量操作必须加锁
   while(ready==false){//若加完锁发现条件还没准备好
        cond_wait();//
        //在condwait()内部有以下逻辑：
       // 1、unlock(); 因为还没准备好，消费者干不了什么，必须把锁让出来，让生产者去准备数据
       // 2、sleep();消费者现在没事可干，开始睡眠（阻塞），去排队，等待生产者准备好了来通知. 1-2必须绑定并原子操作
       // 3、lock();收到了准备好的通知，这个消费者线程在等待队列上被唤醒，开始处理前对数据要加锁
       }
   deal();//处理数据
   unlock();//处理完了，解锁
   }
```
   
生产者线程中，pthread_cond_signal既可以放在pthread_mutex_lock和pthread_mutex_unlock之间，也可以放在pthread_mutex_lock和pthread_mutex_unlock之后，但是各有有缺点。   
**cond_signal在之间**：在某些线程的实现中，会造成等待线程从内核中唤醒（由于cond_signal)然后又回到内核空间（因为cond_wait返回后会有原子加锁的 行为），所以一来一回会有性能的问题。但是在LinuxThreads或者NPTL里面，就不会有这个问题，因为在Linux 线程中，有两个队列，分别是cond_wait队列和mutex_lock队列， cond_signal只是让线程从cond_wait队列移到mutex_lock队列，而不用返回到用户空间，不会有性能的损耗。所以在Linux中推荐使用这种模式。  
**cond_signal在之后**：
优点：不会出现之前说的那个潜在的性能损耗，因为在signal之前就已经释放锁了  
缺点：如果unlock和signal之前，有个低优先级的线程正在mutex上等待的话，那么这个低优先级的线程就会抢占高优先级（cond_wait的线程)的线程，即锁被别人抢了，而这在上面的放中间的模式下是不会出现的。  

**问题1**：为什么cond_wait()中步骤 1-2要原子性？  
    如果不是原子性的，上面的两个步骤中间就可能插入其它操作。比如，如果先释放mutex，这时候生产者线程向队列中添加数据，然后signal,之后消费者线程才去『把调用线程放到等待队列上睡眠』，signal信号就这样被丢失了。
    
**问题2**：消费者线程中while换成if可不可以？  
    答案是不可以，一个生产者可能对应着多个消费者，生产者向队列中插入一条数据之后发出signal，然后某一个消费者线程的pthread_cond_wait获取mutex后返回，然后进行处理，其它线程会pending在这里，处理线程处理完毕之后释放mutex，刚才等待的另一个消费者线程获取mutex，但是此时队列里没有数据了，如果这里用if，就会在当前队列为空的状态下继续往下处理，这显然是不合理的。而while能够再判断一次，继续阻塞在condwait上。
    
        






