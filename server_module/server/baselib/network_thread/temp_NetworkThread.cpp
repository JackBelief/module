
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <netinet/tcp.h>
#include <errno.h>

#include <iostream>
using namespace std;


#include "NetworkThread.h"
#include "../multi_threads/ThreadPoolManager.h"

const int g_server_port = 50022;
const int g_max_open_file_count = 80;                            // 控制当前进程可以打开的文件个数（包括socket、管道等）
const int g_max_connection_count = 20;                           // 监听队列大小
const int g_max_epoll_event_count = 50;                          // epoll可以监听的事件个数
const std::string g_ip_addr = "121.42.161.154";

void NetworkThread::join()
{
    pthread_join(_tid, NULL);
    return;
}

bool NetworkThread::init()
{
    return prepare() && create() && process();
}
void NetworkThread::stop()
{
    _is_stop = true;
    return;
}

bool NetworkThread::prepare()
{
    // 针对客户端关闭连接，服务器还要回复数据的情况，此时服务器会收到SIGPIPE信号，该信号会终止进程

    // 当客户端对socket执行close或者shutdown+SHUT_RD(半关闭读端)时，如果服务端在执行写操作，则会收到SIGPIPE信号，
    // 该信号的作用是将写进程终止，所以此处要对该信号忽略
    signal(SIGPIPE, SIG_IGN);

    // 修改进程可打开的文件数量，此处就是可接受的请求数
    struct rlimit file_limit;
    file_limit.rlim_cur = g_max_open_file_count;
    file_limit.rlim_max = g_max_open_file_count;
    if (-1 == setrlimit(RLIMIT_NOFILE, &file_limit))
    {
        std::cout << "reset process open file limit fail" << std::endl;
        return false;
    }

    // 空闲连接处理??

    return true;
}

bool NetworkThread::create()
{
    // step 1 : 创建套接字
    // 指定地址域(IPv4)；数据类型(字节流)；协议类型(TCP)
    _server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (-1 == _server_sock)    
    {
        std::cout << "server socket create fail" << std::endl;
        return false;
    }

    // 充许绑定已被使用的地址、端口号（前提是TIME_WAIT状态），目的是快速重用socket
//    int opt = 1;
//    setsockopt(_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 为服务绑定地址
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(g_server_port);
    server_addr.sin_addr.s_addr = inet_addr(g_ip_addr.c_str());
    if (-1 == bind(_server_sock, (sockaddr*)&server_addr, sizeof(server_addr)))
    {
        std::cout << "server socket bind fail" << std::endl;
        return false;
    }

    // 端口监听
    if (-1 == listen(_server_sock, g_max_connection_count))
    {
        std::cout << "server socket listen fail" << std::endl;
        return false;
    }
    std::cout << "server socket create success svrsock=" << _server_sock << std::endl;

    // step 2 : 创建epoll句柄
    _server_epoll = epoll_create(g_max_epoll_event_count);
    if (-1 == _server_epoll)
    {
        std::cout << "server epoll create fail" << std::endl;
        return false;
    }

    // step 3 : 创建libevent实例
    _lib_base = event_base_new();
    if (!_lib_base)
    {
        std::cout << "lib event base create fail" << std::endl;
        return false;
    }

    return true;
}

bool NetworkThread::process()
{
    // 创建网络线程
    if (-1 == pthread_create(&_tid, NULL, &NetworkThread::run, this))
    {
        std::cout << "create network thread fail" << std::endl;
        return false;
    }

    return true;
}

void* NetworkThread::run(void* para)
{
    struct event_base*              _base;
    NetworkThread* pNet = (NetworkThread*)para;

    // 注册服务器套接字(EPOLLET是边沿触发、EPOLLLT是电平触发)
    if (!pNet->register_event(pNet->_server_epoll, pNet->_server_sock, EPOLLIN | EPOLLET))
    {
        return NULL;
    }

    // 监听服务
    while (!(pNet->_is_stop))
    {
        epoll_event event_arr[g_max_epoll_event_count];
        int event_num = epoll_wait(pNet->_server_epoll, event_arr, sizeof(event_arr), 100);
        for (int uli = 0; uli < event_num; ++uli)
        {
            int temp_sock = event_arr[uli].data.fd;
            if (pNet->_server_sock == temp_sock)
            {
                // 接受客户端请求，并注册到epoll
                int conn_sock = accept(pNet->_server_sock, NULL, 0);
                if (-1 == conn_sock)
                {
                    std::cout << "accept client socket fail errno=" << errno << std::endl;
                    continue;
                }

                std::cout << "accept client request conn_sock=" << conn_sock << std::endl;

#if 0
int keepAlive = 1;    
int keepIdle = 5;        //默认为7200即2小时
int keepInterval = 5;    //默认75秒
int keepCount = 1;       //默认为9次
setsockopt(conn_sock,SOL_SOCKET,SO_KEEPALIVE,(void*)&keepAlive,sizeof(keepAlive));

setsockopt(conn_sock,SOL_TCP,TCP_KEEPIDLE,(void *)&keepIdle,sizeof(keepIdle));

setsockopt(conn_sock,SOL_TCP,TCP_KEEPINTVL,(void *)&keepInterval,sizeof(keepInterval));

setsockopt(conn_sock,SOL_TCP,TCP_KEEPCNT,(void *)&keepCount,sizeof(keepCount));
#endif
                pNet->register_event(pNet->_server_epoll, conn_sock, EPOLLIN | EPOLLET);
            }
            else
            {
                pNet->process_event(event_arr[uli]);
            }
        }
    }

    return NULL;
}

bool NetworkThread::register_event(int epoll, int sock, uint32_t reg_events)
{
    if (-1 == fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK))
    {
        std::cout << "set socket noblock fail sock=" << sock << std::endl;
        return false;
    }

    epoll_event event;
    event.data.fd = sock;
    event.events = reg_events;
    if (-1 == epoll_ctl(epoll, EPOLL_CTL_ADD, sock, &event))
    {
        std::cout << "epoll event register fail sock=" << sock << std::endl;
        return false;
    }

    return true;
}

void NetworkThread::process_event(const epoll_event& event)
{
    if (event.events & EPOLLIN)
    {
        // 读事件处理
        int event_sock = event.data.fd;
        const int read_buff_size = 1000;
        char read_data[read_buff_size] = {0};

        while (true)
        {
            int read_rst = recv(event_sock, read_data, sizeof(read_data), 0);
            if (0 == read_rst)
            {
                std::cout << "sdfsd" << std::endl;
                break;
                // 将套接字从epoll集合中删除；然后在关闭套接字
                std::cout << "sender shutdown socket, we'll close socket sock=" << event_sock << std::endl;
                epoll_event temp_event = event;
                epoll_ctl(_server_epoll, EPOLL_CTL_DEL, event_sock, &temp_event);
                close(event_sock);
                break;
            }
            else if (-1 == read_rst)
            {
                if (EAGAIN == errno)
                {
                    std::cout << "read over and read all data sock=" << event_sock << " data=" << read_data << std::endl;
                    // 数据读取完毕
//                    GThreadPoolManager::instance().push_msg("common_work", [&](){std::cout << "read over and read all data sock=" << event_sock << " data=" << read_data << std::endl;});
                    break;
                }
                else
                {
                    // 数据读取异常
                    std::cout << "read data error sock=" << event_sock << " errno=" << errno << std::endl;
                    break;
                }
            }
						else
						{
								//std::cout << "read data success size=" << read_rst << std::endl;
            }
        }
    }

    std::cout << "process success" << "ETIMEOUT=" << ETIMEDOUT << " ECONNRESET=" << ECONNRESET << " EHOSTUNREACH=" << EHOSTUNREACH << std::endl;
    return;
}


