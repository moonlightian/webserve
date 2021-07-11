# 工作线程中的主从状态机
各子线程通过process函数对任务进行处理，调用process_read函数和process_write函数分别完成报文解析与报文响应两个任务。

 # process 函数
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

 # process_read() 函数
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
        //m_start_line是每一个数据行在m_read_buf中的起始位置
       //m_checked_idx表示从状态机在m_read_buf中读取的位置,都是相对位置
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
那么，这里的判断条件为什么要写成这样呢？

在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可。

但，在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件。

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
//m_read_idx指向缓冲区m_read_buf的数据末尾的下一个字节,即buf长度
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

主状态机的初始状态，调用parse_request_line函数解析请求行

解析函数从m_read_buf中解析HTTP请求行，获得请求方法、目标URL及HTTP版本号

解析完成后主状态机的状态变为CHECK_STATE_HEADER



## CHECK_STATE_HEADER
解析完请求行后，主状态机继续分析请求头。在报文中，请求头和空行的处理使用的同一个函数，这里通过判断当前的text首位是不是\0字符，若是，则表示当前处理的是空行，若不是，则表示当前处理的是请求头。

调用parse_headers函数解析请求头部信息

判断是空行还是请求头，若是空行，进而判断content-length是否为0，如果不是0，表明是POST请求，则状态转移到CHECK_STATE_CONTENT，否则说明是GET请求，则报文解析结束。

若解析的是请求头部字段，则主要分析connection字段，content-length字段，其他字段可以直接跳过，各位也可以根据需求继续分析。

connection字段判断是keep-alive还是close，决定是长连接还是短连接

content-length字段，这里用于读取post请求的消息体长度

## CHECK_STATE_CONTENT

仅用于解析POST请求，调用parse_content函数解析消息体

用于保存post请求消息体，为后面的登录和注册做准备
