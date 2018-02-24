
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <event.h>
#include <iostream>
using namespace std;


#include "NetworkThread.h"
#include "../multi_threads/ThreadPoolManager.h"

const int g_server_port = 50022;
const int g_max_open_file_count = 8000;                            // 控制当前进程可以打开的文件个数（包括socket、管道等）
const int g_listen_queue_size = 2000;                           // 监听队列大小
const int g_max_epoll_event_count = 5000;                          // epoll可以监听的事件个数
const std::string g_ip_addr = "121.42.161.154";

void NetworkThread::join()
{
    pthread_join(_tid, NULL);
    return;
}

bool NetworkThread::init()
{
    prepare() && create() && process();
    return true;
}
void NetworkThread::stop()
{
    _is_stop = true;
    return;
}

bool NetworkThread::prepare()
{
    // 修改进程可打开的文件数量，此处就是可接受的请求数
    struct rlimit file_limit;
    file_limit.rlim_cur = g_max_open_file_count;
    file_limit.rlim_max = g_max_open_file_count;
    if (-1 == setrlimit(RLIMIT_NOFILE, &file_limit))
    {
        std::cout << "reset process open file limit fail" << std::endl;
        return false;
    }

    return true;
}

// 监听回调函数——重新注册连接套接字事件
// 注意：回调函数是静态函数
void NetworkThread::listen_callback(struct evconnlistener* listener, evutil_socket_t conn_sock, struct sockaddr* conn_addr, int addr_len, void* arg)
{
    struct event_base* base = (event_base*)arg;                           // 获取礼拜event实例

    // BEV_OPT_CLOSE_ON_FREE表示当回收bufferevent空间时(bufferevent_free)，对应的连接套接字也会被关闭的，监听连接套接字
    struct bufferevent* buff = bufferevent_socket_new(base, conn_sock, BEV_OPT_CLOSE_ON_FREE);
    if (!buff)
    {
        std::cout << "create socket buff fail conn_sock=" << conn_sock << std::endl;
        event_base_loopbreak(base);
        return;
    }

    // 为socket的buff设置回调函数
    bufferevent_setcb(buff, conn_readcb, conn_writecb, conn_eventcb, NULL);

    // 为socket的buff设置事件
    // 对于buff而言，事件和回调函数的设置是两把事
    bufferevent_enable(buff, EV_READ | EV_WRITE);
    bufferevent_setwatermark(buff, EV_READ, 0, 0);

    return;
}

// 读回调函数
void NetworkThread::conn_readcb(struct bufferevent* bev, void *user_data)
{
    int end_index = 0;
    char rdbuff[32] = {0};
    while (true)  
    {
        size_t count = bufferevent_read(bev, (void*)(rdbuff + end_index), sizeof(rdbuff) - end_index);
        if (count > 0)
        {
            end_index += count;
        }
        else
        {
            break;
        }
    }

    if (1 == end_index)
    {
        return;
    }

    GThreadPoolManager::instance().push_msg("common_work", [=](){
                                                                    std::string str = rdbuff;
                                                                    str.pop_back();
                                                                    str.append("tqy");
                                                                    std::cout << str << std::endl;
                                                                    // bufferevent_write会触发写事件
                                                                    GThreadPoolManager::instance().push_msg("response_work", [=](){bufferevent_write(bev, str.c_str(), str.size());});
                                                                });
    return;
}

// 触发写事件时，会调用该写函数
void NetworkThread::conn_writecb(struct bufferevent* bev, void *user_data)
{
    std::cout << "123" << std::endl;
    return;
}

// 其他事件的处理
void NetworkThread::conn_eventcb(struct bufferevent* bev, short events, void *user_data)
{
    evutil_socket_t fd = bufferevent_getfd(bev);
    std::string out_str = "fd=" + std::to_string(fd) + " ";

    if (events & BEV_EVENT_TIMEOUT)
    {
        out_str += "time out";
    }
    else if (events & BEV_EVENT_EOF)
    {
        out_str += "connection closed";
    }
    else if (events & BEV_EVENT_ERROR)
    {
        out_str += "other error";
    }

    std::cout << out_str << std::endl;
    bufferevent_free(bev);                     // 当前连接套接字的空间被释放后，该连接套接字也会被关闭
    return;
}

bool NetworkThread::create()
{
    // step 1 : 创建libevent实例
    _lib_base = event_init();
    if (!_lib_base)
    {
        std::cout << "libevent base create fail" << std::endl;
        return false;
    }

    // step 2 : 创建链接监听对象
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(g_server_port);
    server_addr.sin_addr.s_addr = inet_addr(g_ip_addr.c_str());

    // LEV_OPT_REUSEABLE确保端口的快速重用；LEV_OPT_CLOSE_ON_FREE释放连接监听器会关闭底层套接字
    _ev_listener = evconnlistener_new_bind(_lib_base, listen_callback, (void*)_lib_base, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, g_listen_queue_size, (sockaddr*)&server_addr, sizeof(server_addr));
    if (!_ev_listener)
    {
        std::cout << "listener create fail" << std::endl;
        return false;
    }

    _timer_ev = evtimer_new(_lib_base, timer_stop_callback, this);                // 生成定时事件
    evtimer_add(_timer_ev, &_timer_inv);                                          // 注册定时事件

    return true;
}

// 定时查询是否要退出libevent，该处的作用就是定时唤醒被挂起的线程
void NetworkThread::timer_stop_callback(evutil_socket_t fd, short events, void* arg)
{
    NetworkThread* pNet = (NetworkThread*)arg;
    if (pNet->_is_stop)
    {
        event_base_loopbreak(pNet->_lib_base);               // libevent实例退出
    }
    else if (!evtimer_pending(pNet->_timer_ev, NULL))
    {
        evtimer_del(pNet->_timer_ev);
        evtimer_add(pNet->_timer_ev, &(pNet->_timer_inv)); 
    }

    return;
}

bool NetworkThread::process()
{
    // 创建网络线程，监听请求
    if (-1 == pthread_create(&_tid, NULL, &NetworkThread::run, this))
    {
        std::cout << "create network thread fail" << std::endl;
        return false;
    }

    return true;
}

void* NetworkThread::run(void* para)
{
    NetworkThread* pNet = (NetworkThread*)para;

    // 启动libevent，其中，event_base_dispatch同于没有设置标志的event_base_loop
    event_base_dispatch(pNet->_lib_base);

    // loop循环结束后，空间释放
    evconnlistener_free(pNet->_ev_listener);    
    event_base_free(pNet->_lib_base);

    return NULL;
}



