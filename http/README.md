# epoll
epoll_ctl函数
```cpp
#include <sys/epoll.h>
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
```
epfd：为epoll_creat的句柄，指向内核中的事件表

op：表示动作，用3个宏来表示：

EPOLL_CTL_ADD (注册新的fd到epfd)，

EPOLL_CTL_MOD (修改已经注册的fd的监听事件)，

EPOLL_CTL_DEL (从epfd删除一个fd)；

event：告诉内核需要监听的事件
```cpp
struct epoll_event {
__uint32_t events; /* Epoll events */
epoll_data_t data; /* User data variable */
};
```
events描述事件类型，其中epoll事件类型有以下几种

EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭连接）

EPOLLOUT：表示对应的文件描述符可以写

EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）

EPOLLERR：表示对应的文件描述符发生错误

EPOLLHUP：表示对应的文件描述符被挂断；

EPOLLET：将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于默认的水平触发(Level Triggered)而言的，必须设置成非阻塞

EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里，重置EPOLLONESHOT

# LT ET
## 水平触发 LT
1. 对于读操作
只要缓冲内容不为空，LT模式返回读就绪，直到你读完。

2. 对于写操作
只要缓冲区还不满，LT模式会返回写就绪。

## 边缘触发 ET
1. 对于读操作，
（1）当缓冲区由不可读变为可读的时候，即缓冲区由空变为不空的时候。

（2）当有新数据到达时，即缓冲区中的待读数据变多的时候。

（3）当缓冲区有数据可读，且应用进程对相应的描述符进行EPOLL_CTL_MOD 修改EPOLLIN事件时。

2. 对于写操作
（1）当缓冲区由不可写变为可写时。

（2）当有旧数据被发送走，即缓冲区中的内容变少的时候。

（3）当缓冲区有空间可写，且应用进程对相应的描述符进行EPOLL_CTL_MOD 修改EPOLLOUT事件时。

一个管道收到了1kb的数据,epoll会立即返回,此时读了512字节数据,然后再次调用epoll.
这时如果是水平触发的,epoll会立即返回,因为有数据准备好了.
如果是边缘触发的不会立即返回,因为此时虽然有数据可读但是已经触发了一次通知,在这次通知到现在还没有新的数据到来,直到有新的数据到来epoll才会返回,此时老的数据和新的数据都可以读取到(当然是需要这次你尽可能的多读取).
所以当我们写epoll网络模型时，如果我们用水平触发不用担心数据有没有读完因为下次epoll返回时，没有读完的socket依然会被返回，但是要注意这种模式下的写事件，因为是水平触发，每次socket可写时epoll都会返回，当我们写的数据包过大时，一次写不完，要多次才能写完或者每次socket写都写一个很小的数据包时，每次写都会被epoll检测到，因此长期关注socket写事件会无故cpu消耗过大甚至导致cpu跑满，

## 为何 epoll 的 ET 模式一定要设置为非阻塞IO？
ET模式下每次write或read需要循环write或read直到返回EAGAIN错误。以读操作为例，这是因为ET模式只在socket描述符状态发生变化时才触发事件，如果不一次把socket内核缓冲区的数据读完，会导致socket内核缓冲区中即使还有一部分数据，该socket的可读事件也不会被触发
根据上面的讨论，若ET模式下使用阻塞IO，则程序一定会阻塞在最后一次write或read操作，因此说ET模式下一定要使用非阻塞IO
## EPOLLONESHOT

一个线程读取某个socket上的数据后开始处理数据，在处理过程中该socket上又有新数据可读，此时另一个线程被唤醒读取，此时出现两个线程处理同一个socket
我们期望的是一个socket连接在任一时刻都只被一个线程处理，通过epoll_ctl对该文件描述符注册epolloneshot事件，一个线程处理socket时，其他线程将无法处理，当该线程处理完后，需要通过epoll_ctl重置epolloneshot事件

# HTTP报文格式
HTTP报文分为请求报文和响应报文两种，每种报文必须按照特有格式生成，才能被浏览器端识别。

其中，浏览器端向服务器发送的为请求报文，服务器处理后返回给浏览器端的为响应报文。

## 请求报文
HTTP请求报文由请求行（request line）、请求头部（header）、空行和请求数据四个部分组成。

项目中，请求分为两种，GET和POST，具体的：

### GET
```cpp
// 每行末尾都有\r\n

 1    GET /562f25980001b1b106000338.jpg HTTP/1.1
 2    Host:img.mukewang.com
 3    User-Agent:Mozilla/5.0 (Windows NT 10.0; WOW64)
 4    AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36
 5    Accept:image/webp,image/*,*/*;q=0.8
 6    Referer:http://www.imooc.com/
 7    Accept-Encoding:gzip, deflate, sdch
 8    Accept-Language:zh-CN,zh;q=0.8
 9    空行
10    请求数据为空
```

### POST

```cpp

1    POST  HTTP/1.1
2    Host:www.wrox.com
3    User-Agent:Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 2.0.50727; .NET CLR 3.0.04506.648; .NET CLR 3.5.21022)
4    Content-Type:application/x-www-form-urlencoded
5    Content-Length:40
6    Connection: Keep-Alive
7    空行
8    name=Professional%20Ajax&publisher=Wiley
```
请求行，用来说明请求类型,要访问的资源以及所使用的HTTP版本。
GET说明请求类型为GET，/562f25980001b1b106000338.jpg(URL)为要访问的资源，该行的最后一部分说明使用的是HTTP1.1版本。

请求头部，紧接着请求行（即第一行）之后的部分，用来说明服务器要使用的附加信息。

HOST，给出请求资源所在服务器的域名。

User-Agent，HTTP客户端程序的信息，该信息由你发出请求使用的浏览器来定义,并且在每个请求中自动发送等。

Accept，说明用户代理可处理的媒体类型。

Accept-Encoding，说明用户代理支持的内容编码。

Accept-Language，说明用户代理能够处理的自然语言集。

Content-Type，说明实现主体的媒体类型。

Content-Length，说明实现主体的大小。

Connection，连接管理，可以是Keep-Alive或close。

空行，请求头部后面的空行是必须的即使第四部分的请求数据为空，也**必须有空行**。

请求数据也叫主体，可以添加任意的其他数据。

## 响应报文
HTTP响应也由四个部分组成，分别是：状态行、消息报头、空行和响应正文。
```cpp
 1HTTP/1.1 200 OK
 2Date: Fri, 22 May 2009 06:07:21 GMT
 3Content-Type: text/html; charset=UTF-8
 4空行
 5<html>
 6      <head></head>
 7      <body>
 8            <!--body goes here-->
 9      </body>
10</html>
```
状态行，由HTTP协议版本号， 状态码， 状态消息 三部分组成。
第一行为状态行，（HTTP/1.1）表明HTTP版本为1.1版本，状态码为200，状态消息为OK。

消息报头，用来说明客户端要使用的一些附加信息。
第二行和第三行为消息报头，Date:生成响应的日期和时间；Content-Type:指定了MIME类型的HTML(text/html),编码类型是UTF-8。

空行，消息报头后面的**空行是必须的**。

响应正文，服务器返回给客户端的文本信息。空行后面的html部分为响应正文。


# 有限状态机
有限状态机，是一种抽象的理论模型，它能够把有限个变量描述的状态变化过程，以可构造可验证的方式呈现出来。比如，封闭的有向图。
有限状态机可以通过if-else,switch-case和函数指针来实现，从软件工程的角度看，主要是为了封装逻辑。

# http连接处理流程

浏览器端发出http连接请求，服务器端主线程创建http对象接收请求并将所有数据从socket缓冲区读入对应buffer，将该对象插入任务队列后，工作线程从任务队列中取出一个任务进行处理。

各工作线程通过process函数对任务进行处理，调用process_read函数和process_write函数分别完成报文解析与报文响应两个任务。

process_read通过while循环，将【主 从】状态机进行封装，从buf读出报文，对报文的每一行进行循环处理。
     > * 从状态机读取数据,更新自身状态和接收数据,传给主状态机
     > * 主状态机根据从状态机状态,更新自身状态,决定响应请求还是继续读取



循环体

从状态机读取数据

调用get_line函数，通过m_start_line将从状态机读取数据间接赋给text

主状态机解析text

# http类
该类对象由主线程创建，read_once()函数调用recv()读取浏览器发来的全部数据存到buffer
（1）调用构造函数初始化这对象，记录sockfd等属性；
（2）read_once()函数读取浏览器发来的全部数据存到buffer
```cpp
//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完，读取到m_read_buffer中
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)//m_read_idx 已经读到buf的数据
    {
        return false;
    }
    int bytes_read = 0;

#ifdef connfdLT //LT模式，不用一次性读完
    //从内核buffer拷贝数据到用户层buffer
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    m_read_idx += bytes_read;//更新已经读到buf的数据

    if (bytes_read <= 0)
    {
        return false;
    }

    return true;

#endif

#ifdef connfdET //ET模式,循环读取直到读完
    while (true)
    {   //成功执行时，返回接收到的字节数;另一端已关闭则返回0;失败返回-1，errno被设为以下的某个值 ：https://blog.csdn.net/mercy_ps/article/details/82224128
        //https://www.cnblogs.com/ellisonzhang/p/10412021.html
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)//非阻塞模式下的两种情况，表明et已经读完
                break;
            return false;
        }
        else if (bytes_read == 0)//另一端已关闭则返回0
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
#endif
}
```
# recv函数
int recv( SOCKET s, char FAR *buf, int len, int flags);

每个TCP socket在内核中都有一个发送缓冲区和一个接收缓冲区，TCP的全双工的工作模式以及TCP的流量(拥塞)控制便是依赖于这两个独立的buffer以及buffer的填充状态。接收缓冲区把数据缓存入内核，应用进程一直没有调用recv()进行读取的话，此数据会一直缓存在相应socket的接收缓冲区内。再啰嗦一点，不管进程是否调用recv()读取socket，对端发来的数据都会经由内核接收并且缓存到socket的内核接收缓冲区之中。recv()所做的工作，就是把内核缓冲区中的数据拷贝到应用层用户的buffer里面，并返回，仅此而已。  
默认 socket 是阻塞的，阻塞与非阻塞recv返回值没有区分，都是 <0 出错， =0 连接关闭， >0 接收到数据大小。

失败返回-1，errno被设为以下的某个值 ：
EAGAIN：套接字已标记为非阻塞，而接收操作被阻塞或者接收超时 
EBADF：sock不是有效的描述词 
ECONNREFUSE：远程主机阻绝网络连接 
EFAULT：内存空间访问出错 
EINTR：操作被信号中断 
EINVAL：参数无效 
ENOMEM：内存不足 
ENOTCONN：与面向连接关联的套接字尚未被连接上 
ENOTSOCK：sock索引的不是套接字 当返回值是0时，为正常关闭连接；
EWOULDBLOCK：用于非阻塞模式，不需要重新读或者写


# 工作线程中的主从状态机
各子线程通过process函数对任务进行处理，调用process_read函数和process_write函数分别完成报文解析与报文响应两个任务。

 ## process 函数
 ```cpp
 1void http_conn::process()
 2{
 3    HTTP_CODE read_ret=process_read();
 4
 5    //NO_REQUEST，表示请求不完整，需要继续接收请求数据
 6    if(read_ret==NO_REQUEST)
 7    {
 8        //注册并监听读事件
 9        modfd(m_epollfd,m_sockfd,EPOLLIN);
10        return;
11    }
12
13    //调用process_write完成报文响应
14    bool write_ret=process_write(read_ret);
15    if(!write_ret)
16    {
17        close_conn();
18    }
19    //注册并监听写事件
20    modfd(m_epollfd,m_sockfd,EPOLLOUT);
21}
```

 ## process_read() 函数
 从状态机负责读取报文的一行，主状态机负责对该行数据进行解析，主状态机内部调用从状态机，从状态机驱动主状态机
 通过while循环，将主从状态机进行封装，对报文的每一行进行循环处理。
 
 HTTP_CODE含义:  表示HTTP请求的处理结果，在头文件中初始化了八种情形，在报文解析时只涉及到四种。

NO_REQUEST:请求不完整，需要继续读取请求报文数据  
GET_REQUEST:获得了完整的HTTP请求  
BAD_REQUEST:HTTP请求报文有语法错误  
INTERNAL_ERROR:服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发  
#  状态机总逻辑
 ```cpp
 //
 //m_start_line是行在buffer中的起始位置，将该位置后面的数据赋给text
 //此时从状态机已提前将一行的末尾字符\r\n变为\0\0，所以text可以直接取出完整的行进行解析
 char* get_line(){
     return m_read_buf+m_start_line;
 }
 
http_conn::HTTP_CODE http_conn::process_read()
{    //初始化从状态机状态、HTTP请求解析结果
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    //这里为什么要写两个判断条件？第一个判断条件为什么这样写？\
    //parse_line为从状态机的具体实现
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        //m_start_line是每一个数据行在m_read_buf中的起始位置，是不断推进的
       //m_checked_idx表示从状态机在m_read_buf中读取的位置,即下一次循环要开始的位置，都是相对位置
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        Log::get_instance()->flush();
        
        //主状态机的三种状态转移逻辑
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE: //解析请求行
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER://解析请求头
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
             //get请求没有主体，返回GET_REQUEST，完整解析GET请求后，跳转到报文响应函数
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT://解析消息体
        {   
            ret = parse_content(text);
            if (ret == GET_REQUEST)//完整解析POST请求后，跳转到报文响应函数
                return do_request();
            //解析完消息体即完成报文解析，避免再次进入循环，更新line_status
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}
```
## 那么，这里的判断条件为什么要写成这样呢？

在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可。

但，在POST请求报文中，**消息体的末尾没有任何字符**，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件。

那后面的&& line_status==LINE_OK又是为什么？

解析完消息体后，报文的完整解析就完成了，但此时主状态机的状态还是CHECK_STATE_CONTENT，也就是说，符合循环入口条件，还会再次进入循环，这并不是我们所希望的。

为此，增加了该语句，并在完成消息体解析后，将line_status变量更改为LINE_OPEN，此时可以跳出循环，完成报文解析任务。

# 从状态机逻辑

在HTTP报文中，每一行的数据由\r\n作为结束字符，空行则是仅仅是字符\r\n。因此，可以通过查找\r\n将报文拆解成单独的行进行解析，项目中便是利用了这一点。  

从状态机负责读取buffer中的数据，将每行数据末尾的\r\n置为\0\0，并更新从状态机在buffer中读取的位置m_checked_idx，以此来驱动主状态机解析。  

从状态机从m_read_buf中逐字节读取，判断当前字节是否为\r

接下来的字符是\n，将\r\n修改成\0\0，将m_checked_idx指向下一行的开头，则返回LINE_OK

接下来达到了buffer末尾，表示buffer还需要继续接收，返回LINE_OPEN

否则，表示语法错误，返回LINE_BAD

当前字节不是\r，判断是否是\n（一般是上次读取到\r就到了buffer末尾，没有接收完整，再次接收时会出现这种情况）

如果前一个字符是\r，则将\r\n修改成\0\0，将m_checked_idx指向下一行的开头，则返回LINE_OK

当前字节既不是\r，也不是\n

表示接收不完整，需要继续接收，返回LINE_OPEN
```cpp
//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
//m_read_idx指向缓冲区m_read_buf的数据末尾的下一个字节,即表示buf长度
 //m_checked_idx指向从状态机当前正在分析的字节
 
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;//temp为将要分析的字节
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')//如果当前是\r字符，则有可能会读取到完整行
        {
            if ((m_checked_idx + 1) == m_read_idx)//下一个字符达到了buffer结尾，则接收不完整，需要继续接收
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')//下一个字符是\n，将\r\n改为\0\0
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;//如果都不符合，则返回语法错误
        }
        else if (temp == '\n')//如果当前字符是\n，也有可能读取到完整行
       //一般是上次读取到\r就到buffer末尾了，没有接收完整，再次接收时会出现这种情况
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')//前一个字符是\r，则接收完整
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //并没有找到\r\n，需要继续接收
    return LINE_OPEN;
}

```

# 主状态机逻辑
主状态机初始状态是CHECK_STATE_REQUESTLINE，通过调用从状态机来驱动主状态机，在主状态机进行解析前，从状态机已经将每一行的末尾\r\n符号改为\0\0，以便于主状态机直接取出对应字符串进行处理。
## CHECK_STATE_REQUESTLINE

主状态机的初始状态，调用parse_request_line()函数解析请求行

解析函数从m_read_buf中解析HTTP请求行，获得请求方法、目标URL及HTTP版本号

解析完成后主状态机的状态变为CHECK_STATE_HEADER

```cpp
//解析http请求行，获得请求方法，目标url及http版本号
 2http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
 3{
 4    //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本，其中各个部分之间通过\t或空格分隔。
 5    //请求行中最先含有空格和\t任一字符的位置并返回
 6    m_url=strpbrk(text," \t");//比较字符串str1中是否有str2中的字符。如果找到，则返回str1中该字符位置的指针。
 7
 8    //如果没有空格或\t，则报文格式有误
 9    if(!m_url)
10    {
11        return BAD_REQUEST;
12    }
13
14    //将该位置改为\0，用于将前面数据取出
15    *m_url++='\0';
16
17    //取出数据，并通过与GET和POST比较，以确定请求方式
18    char *method=text;
19    if(strcasecmp(method,"GET")==0)
20        m_method=GET;
21    else if(strcasecmp(method,"POST")==0)
22    {
23        m_method=POST;
24        cgi=1;
25    }
26    else
27        return BAD_REQUEST;
28
29    //m_url此时跳过了第一个空格或\t字符，但不知道之后是否还有
30    //将m_url向后偏移，通过查找，继续跳过空格和\t字符，指向请求资源的第一个字符
31    m_url+=strspn(m_url," \t");//检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
32
33    //使用与判断请求方式的相同逻辑，判断HTTP版本号
34    m_version=strpbrk(m_url," \t");
35    if(!m_version)
36        return BAD_REQUEST;
37    *m_version++='\0';
38    m_version+=strspn(m_version," \t");
39
40    //仅支持HTTP/1.1
41    if(strcasecmp(m_version,"HTTP/1.1")!=0)
42        return BAD_REQUEST;
43
44    //对请求资源前7个字符进行判断
45    //这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理
46    if(strncasecmp(m_url,"http://",7)==0)
47    {
48        m_url+=7;
49        m_url=strchr(m_url,'/');
50    }
51
52    //同样增加https情况
53    if(strncasecmp(m_url,"https://",8)==0)
54    {
55        m_url+=8;
56        m_url=strchr(m_url,'/');
57    }
58
59    //一般的不会带有上述两种符号，直接是单独的/或/后面带访问资源
60    if(!m_url||m_url[0]!='/')
61        return BAD_REQUEST;
62
63    //当url为/时，显示欢迎界面
64    if(strlen(m_url)==1)
65        strcat(m_url,"judge.html");
66
67    //请求行处理完毕，将主状态机转移处理请求头
68    m_check_state=CHECK_STATE_HEADER;
69    return NO_REQUEST;
70}
```



## CHECK_STATE_HEADER
解析完请求行后，主状态机继续分析请求头。在报文中，请求头和空行的处理使用的同一个函数，这里通过判断当前的text首位是不是\0字符，若是，则表示当前处理的是空行，若不是，则表示当前处理的是请求头。

调用parse_headers函数解析请求头部信息

判断是空行还是请求头，若是空行，进而判断content-length是否为0，如果不是0，表明是POST请求，则状态转移到CHECK_STATE_CONTENT，否则说明是GET请求，则报文解析结束。

若解析的是请求头部字段，则主要分析connection字段，content-length字段，其他字段可以直接跳过，各位也可以根据需求继续分析。

connection字段判断是keep-alive还是close，决定是长连接还是短连接

content-length字段，这里用于读取post请求的消息体长度
```cpp
 1//解析http请求的一个头部信息
 2http_conn::HTTP_CODE http_conn::parse_headers(char *text)
 3{
 4    //判断是空行还是请求头
 5    if(text[0]=='\0')
 6    {
 7        //判断是GET还是POST请求
 8        if(m_content_length!=0)
 9        {
10            //POST需要跳转到消息体处理状态
11            m_check_state=CHECK_STATE_CONTENT;
12            return NO_REQUEST;
13        }
14        return GET_REQUEST;
15    }
16    //解析请求头部连接字段
17    else if(strncasecmp(text,"Connection:",11)==0)
18    {
19        text+=11;
20
21        //跳过空格和\t字符
22        text+=strspn(text," \t");
23        if(strcasecmp(text,"keep-alive")==0)
24        {
25            //如果是长连接，则将linger标志设置为true
26            m_linger=true;
27        }
28    }
29    //解析请求头部内容长度字段
30    else if(strncasecmp(text,"Content-length:",15)==0)
31    {
32        text+=15;
33        text+=strspn(text," \t");
34        m_content_length=atol(text);
35    }
36    //解析请求头部HOST字段
37    else if(strncasecmp(text,"Host:",5)==0)
38    {
39        text+=5;
40        text+=strspn(text," \t");
41        m_host=text;
42    }
43    else{
44        printf("oop!unknow header: %s\n",text);
45    }
46    return NO_REQUEST;
47}
```
## CHECK_STATE_CONTENT

仅用于解析POST请求，调用parse_content函数解析消息体

用于保存post请求消息体，为后面的登录和注册做准备
```cpp
1//判断http请求是否被完整读入
 2http_conn::HTTP_CODE http_conn::parse_content(char *text)
 3{
 4    //判断buffer中是否读取了消息体
 5    if(m_read_idx>=(m_content_length+m_checked_idx)){
 6
 7        text[m_content_length]='\0';
 8
 9        //POST请求中最后为输入的用户名和密码
10        m_string = text;
11
12        return GET_REQUEST;
13    }
14    return NO_REQUEST;
15}
```
# do_request
process_read函数的返回值是对请求的文件分析后的结果，一部分是语法错误导致的BAD_REQUEST，一部分是do_request的返回结果.
do_request函数将网站根目录和url文件拼接，然后通过stat判断该文件属性。

项目中请求报文解析后的m_url有8种情况。

/ GET请求，跳转到judge.html，即欢迎访问页面  

/0 POST请求，跳转到register.html，即注册页面  

/1 POST请求，跳转到log.html，即登录页面  

/2CGISQL.cgi POST请求，进行登录校验  

验证成功跳转到welcome.html，即资源请求成功页面

验证失败跳转到logError.html，即登录失败页面

/3CGISQL.cgi POST请求，进行注册校验  

注册成功跳转到log.html，即登录页面

注册失败跳转到registerError.html，即注册失败页面

/5 POST请求，跳转到picture.html，即图片请求页面  

/6 POST请求，跳转到video.html，即视频请求页面  

/7 POST请求，跳转到fans.html，即关注页面  

```cpp
//网站根目录，文件夹内存放请求的资源和跳转的html文件
const char* doc_root="/home/qgy/github/ini_tinywebserver/root";

http_conn::HTTP_CODE http_conn::do_request()
{
    //将初始化的m_real_file赋值为网站根目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    
    //printf("m_url:%s\n", m_url);
    //找到m_url中/的位置
    const char *p = strrchr(m_url, '/');

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        //同步线程登录校验
        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {

                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }
    //如果请求资源为/0，表示跳转注册界面
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        //将网站目录和/register.html进行拼接，更新到m_real_file中
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //如果请求资源为/1，表示跳转登录界面
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        //将网站目录和/log.html进行拼接，更新到m_real_file中
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
        
   //通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
    //失败返回NO_RESOURCE状态，表示资源不存在
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    //判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    //判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    //以只读方式获取文件描述符，通过mmap将该文件映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    //避免文件描述符的浪费和占用
    close(fd);
    //表示请求文件存在，且可以访问
    return FILE_REQUEST;
}
```
# process_write
根据do_request的返回状态，服务器子线程调用process_write向m_write_buf中写入响应报文。
HTTP响应由四个部分组成，分别是：状态行、消息报头、空行和响应正文。
```cpp
 1HTTP/1.1 200 OK  //状态行，由HTTP协议版本号， 状态码， 状态消息 三部分组成。（HTTP/1.1）表明HTTP版本为1.1版本，状态码为200，状态消息为OK。
 2Date: Fri, 22 May 2009 06:07:21 GMT //消息报头，用来说明客户端要使用的一些附加信息。
 3Content-Type: text/html; charset=UTF-8//第二行和第三行为消息报头，Date:生成响应的日期和时间；Content-Type:指定了MIME类型的HTML(text/html),编码类型是UTF-8。
 4空行//空行，消息报头后面的空行是必须的。
 5<html>//响应正文，服务器返回给客户端的文本信息。空行后面的html部分为响应正文。
 6      <head></head>
 7      <body>
 8            <!--body goes here-->
 9      </body>
10</html>
```
add_status_line函数，添加状态行：http/1.1 状态码 状态消息

add_headers函数添加消息报头，内部调用add_content_length和add_linger函数

content-length记录响应报文长度，用于浏览器端判断服务器是否发送完数据

connection记录连接状态，用于告诉浏览器端保持长连接

add_blank_line添加空行

```cpp
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    //内部错误，500
    case INTERNAL_ERROR:
    {    //状态行
        add_status_line(500, error_500_title);
        //消息报头
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    //报文语法有误，404
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    //资源没有访问权限，403
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    //文件存在，200
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        //如果请求的资源存在
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
             //第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            //发送的全部数据为响应报文头部信息和文件大小
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else//如果请求的资源大小为0，则返回空白html文件
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    //除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}
```

# http_conn::write 大文件传输有bug
服务器子线程调用process_write完成响应报文写入write buf，随后注册epollout事件。服务器主线程检测写事件，并调用http_conn::write函数将响应报文发送给浏览器端。
该函数具体逻辑如下：

在生成响应报文时初始化byte_to_send，包括头部信息和文件数据大小。通过writev函数循环发送响应报文数据，根据返回值更新byte_have_send和iovec结构体的指针和长度，并判断响应报文整体是否发送成功。  

若writev单次发送成功，更新byte_to_send和byte_have_send的大小，若响应报文整体发送成功,则取消mmap映射,并判断是否是长连接.

长连接重置http类实例，注册读事件，不关闭连接，

短连接直接关闭连接

若writev单次发送不成功，判断是否是写缓冲区满了。

若不是因为缓冲区满了而失败，取消mmap映射，关闭连接

若eagain则满了，更新iovec结构体的指针和长度，并注册写事件，等待下一次写事件触发（当写缓冲区从不可写变为可写，触发epollout），因此在此期间无法立即接收到同一用户的下一请求，但可以保证连接的完整性。
```cpp
bool http_conn::write()
{
    int temp = 0;
    //若要发送的数据长度为0,表示响应报文为空，一般不会出现这种情况
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {   //将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)//判断缓冲区是否满了
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;//更新已发送字节
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}
```










