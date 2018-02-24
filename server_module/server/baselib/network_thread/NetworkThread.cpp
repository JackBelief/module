
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
const int g_max_open_file_count = 8000;                            // ���Ƶ�ǰ���̿��Դ򿪵��ļ�����������socket���ܵ��ȣ�
const int g_listen_queue_size = 2000;                           // �������д�С
const int g_max_epoll_event_count = 5000;                          // epoll���Լ������¼�����
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
    // �޸Ľ��̿ɴ򿪵��ļ��������˴����ǿɽ��ܵ�������
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

// �����ص�������������ע�������׽����¼�
// ע�⣺�ص������Ǿ�̬����
void NetworkThread::listen_callback(struct evconnlistener* listener, evutil_socket_t conn_sock, struct sockaddr* conn_addr, int addr_len, void* arg)
{
    struct event_base* base = (event_base*)arg;                           // ��ȡ���eventʵ��

    // BEV_OPT_CLOSE_ON_FREE��ʾ������bufferevent�ռ�ʱ(bufferevent_free)����Ӧ�������׽���Ҳ�ᱻ�رյģ����������׽���
    struct bufferevent* buff = bufferevent_socket_new(base, conn_sock, BEV_OPT_CLOSE_ON_FREE);
    if (!buff)
    {
        std::cout << "create socket buff fail conn_sock=" << conn_sock << std::endl;
        event_base_loopbreak(base);
        return;
    }

    // Ϊsocket��buff���ûص�����
    bufferevent_setcb(buff, conn_readcb, conn_writecb, conn_eventcb, NULL);

    // Ϊsocket��buff�����¼�
    // ����buff���ԣ��¼��ͻص�������������������
    bufferevent_enable(buff, EV_READ | EV_WRITE);
    bufferevent_setwatermark(buff, EV_READ, 0, 0);

    return;
}

// ���ص�����
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
                                                                    // bufferevent_write�ᴥ��д�¼�
                                                                    GThreadPoolManager::instance().push_msg("response_work", [=](){bufferevent_write(bev, str.c_str(), str.size());});
                                                                });
    return;
}

// ����д�¼�ʱ������ø�д����
void NetworkThread::conn_writecb(struct bufferevent* bev, void *user_data)
{
    std::cout << "123" << std::endl;
    return;
}

// �����¼��Ĵ���
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
    bufferevent_free(bev);                     // ��ǰ�����׽��ֵĿռ䱻�ͷź󣬸������׽���Ҳ�ᱻ�ر�
    return;
}

bool NetworkThread::create()
{
    // step 1 : ����libeventʵ��
    _lib_base = event_init();
    if (!_lib_base)
    {
        std::cout << "libevent base create fail" << std::endl;
        return false;
    }

    // step 2 : �������Ӽ�������
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(g_server_port);
    server_addr.sin_addr.s_addr = inet_addr(g_ip_addr.c_str());

    // LEV_OPT_REUSEABLEȷ���˿ڵĿ������ã�LEV_OPT_CLOSE_ON_FREE�ͷ����Ӽ�������رյײ��׽���
    _ev_listener = evconnlistener_new_bind(_lib_base, listen_callback, (void*)_lib_base, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, g_listen_queue_size, (sockaddr*)&server_addr, sizeof(server_addr));
    if (!_ev_listener)
    {
        std::cout << "listener create fail" << std::endl;
        return false;
    }

    _timer_ev = evtimer_new(_lib_base, timer_stop_callback, this);                // ���ɶ�ʱ�¼�
    evtimer_add(_timer_ev, &_timer_inv);                                          // ע�ᶨʱ�¼�

    return true;
}

// ��ʱ��ѯ�Ƿ�Ҫ�˳�libevent���ô������þ��Ƕ�ʱ���ѱ�������߳�
void NetworkThread::timer_stop_callback(evutil_socket_t fd, short events, void* arg)
{
    NetworkThread* pNet = (NetworkThread*)arg;
    if (pNet->_is_stop)
    {
        event_base_loopbreak(pNet->_lib_base);               // libeventʵ���˳�
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
    // ���������̣߳���������
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

    // ����libevent�����У�event_base_dispatchͬ��û�����ñ�־��event_base_loop
    event_base_dispatch(pNet->_lib_base);

    // loopѭ�������󣬿ռ��ͷ�
    evconnlistener_free(pNet->_ev_listener);    
    event_base_free(pNet->_lib_base);

    return NULL;
}



