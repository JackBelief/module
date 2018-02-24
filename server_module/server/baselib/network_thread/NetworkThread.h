#pragma once

#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <event2/event.h>
#include <event2/listener.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>




#include "../common/Singleton.h"

class NetworkThread
{
    public:
        NetworkThread():_is_stop(false), _timer_inv({1, 0})
        {

        }

        ~NetworkThread()
        {

        }

    public:
        bool init();
        void stop();
        void join();

    private:
        bool prepare();
        bool create();
        bool process();
        bool register_event(int epoll, int sock, uint32_t event);
        void process_event(const epoll_event& event);
        static void listen_callback(struct evconnlistener* listener, evutil_socket_t conn_sock, struct sockaddr* conn_addr, int addr_len, void* arg);
        static void conn_readcb(struct bufferevent *bev, void *user_data);
        static void conn_writecb(struct bufferevent *bev, void *user_data);
        static void conn_eventcb(struct bufferevent *bev, short events, void *user_data);
        static void timer_stop_callback(evutil_socket_t fd, short events, void* arg);
        static void* run(void* para);

    private:
        int                             _server_sock;                     // 服务器套接字
        int                             _server_epoll;                    // 服务器的epoll句柄
        bool                            _is_stop;                         // 线程结束标记
        pthread_t                       _tid;                             // 网络线程ID
        struct timeval                  _timer_inv;                       // 定时事件的时间间隔
        struct event*                   _timer_ev;                        // 定时事件，用于检测退出
        struct event_base*              _lib_base;                        // libevent实例
        struct evconnlistener*          _ev_listener;                     // 监听对象
};

typedef Singleton<NetworkThread> GNetworkThread;



