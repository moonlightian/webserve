
CGI & 数据库连接池
===============
数据库连接池
> * 单例模式，保证唯一
> * list实现连接池
> * 连接池为静态大小
> * 互斥锁实现线程安全

CGI  
> * HTTP请求采用POST方式
> * 登录用户名和密码校验
> * 用户注册及多线程注册安全

## 为什么要创建连接池？

从一般流程中可以看出，若系统需要频繁访问数据库，则需要频繁创建和断开数据库连接，而创建数据库连接是一个很耗时的操作，也容易对数据库造成安全隐患。

在程序初始化的时候，集中创建多个数据库连接，并把他们集中管理，供程序使用，可以保证较快的数据库读写速度，更加安全可靠。

项目使用单例模式和链表创建数据库连接池，实现对数据库连接资源的复用。

项目中的数据库模块分为两部分，其一是数据库连接池的定义，其二是利用连接池完成登录和注册的校验功能。具体的，工作线程从数据库连接池取得一个连接，访问数据库中的数据，访问完毕后将连接交还连接池。

## 通过RAII机制来完成自动销毁连接池
DestroyPool();//关闭链表上的所有数据库连接，清空链表并重置空闲连接和现有连接数量
将此函数放在conn_pool类的析构函数中，析构时自动调用。

## RAII机制释放某个数据库连接
将数据库连接的获取与释放通过RAII机制封装，避免手动释放。

定义
这里需要注意的是，在获取连接时，通过有参构造对传入的参数进行修改。其中数据库连接本身是指针类型，所以参数需要通过双指针才能对其进行修改。
```cpp
 class connectionRAII{
 
 public:
     //双指针对MYSQL *con修改
     connectionRAII(MYSQL **con, connection_pool *connPool);
     ~connectionRAII();
 
 private:
     MYSQL *conRAII;
    connection_pool *poolRAII;
};

 connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
     *SQL = connPool->GetConnection();//从连接池获取一个连接
 
     conRAII = *SQL;
     poolRAII = connPool;
 }
 
 connectionRAII::~connectionRAII(){
     poolRAII->ReleaseConnection(conRAII);//析构时自动释放连接
}
```



