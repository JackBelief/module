
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
const int g_max_open_file_count = 80;                            // ���Ƶ�ǰ���̿��Դ򿪵��ļ�����������socket���ܵ��ȣ�
const int g_max_connection_count = 20;                           // �������д�С
const int g_max_epoll_event_count = 50;                          // epoll���Լ������¼�����
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
    // ��Կͻ��˹ر����ӣ���������Ҫ�ظ����ݵ��������ʱ���������յ�SIGPIPE�źţ����źŻ���ֹ����

    // ���ͻ��˶�socketִ��close����shutdown+SHUT_RD(��رն���)ʱ������������ִ��д����������յ�SIGPIPE�źţ�
    // ���źŵ������ǽ�д������ֹ�����Դ˴�Ҫ�Ը��źź���
    signal(SIGPIPE, SIG_IGN);

    // �޸Ľ��̿ɴ򿪵��ļ��������˴����ǿɽ��ܵ�������
    struct rlimit file_limit;
    file_limit.rlim_cur = g_max_open_file_count;
    file_limit.rlim_max = g_max_open_file_count;
    if (-1 == setrlimit(RLIMIT_NOFILE, &file_limit))
    {
        std::cout << "reset process open file limit fail" << std::endl;
        return false;
    }

    // �������Ӵ���??

    return true;
}

bool NetworkThread::create()
{
    // step 1 : �����׽���
    // ָ����ַ��(IPv4)����������(�ֽ���)��Э������(TCP)
    _server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (-1 == _server_sock)    
    {
        std::cout << "server socket create fail" << std::endl;
        return false;
    }

    // ������ѱ�ʹ�õĵ�ַ���˿ںţ�ǰ����TIME_WAIT״̬����Ŀ���ǿ�������socket
//    int opt = 1;
//    setsockopt(_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Ϊ����󶨵�ַ
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(g_server_port);
    server_addr.sin_addr.s_addr = inet_addr(g_ip_addr.c_str());
    if (-1 == bind(_server_sock, (sockaddr*)&server_addr, sizeof(server_addr)))
    {
        std::cout << "server socket bind fail" << std::endl;
        return false;
    }

    // �˿ڼ���
    if (-1 == listen(_server_sock, g_max_connection_count))
    {
        std::cout << "server socket listen fail" << std::endl;
        return false;
    }
    std::cout << "server socket create success svrsock=" << _server_sock << std::endl;

    // step 2 : ����epoll���
    _server_epoll = epoll_create(g_max_epoll_event_count);
    if (-1 == _server_epoll)
    {
        std::cout << "server epoll create fail" << std::endl;
        return false;
    }

    // step 3 : ����libeventʵ��
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
    // ���������߳�
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

    // ע��������׽���(EPOLLET�Ǳ��ش�����EPOLLLT�ǵ�ƽ����)
    if (!pNet->register_event(pNet->_server_epoll, pNet->_server_sock, EPOLLIN | EPOLLET))
    {
        return NULL;
    }

    // ��������
    while (!(pNet->_is_stop))
    {
        epoll_event event_arr[g_max_epoll_event_count];
        int event_num = epoll_wait(pNet->_server_epoll, event_arr, sizeof(event_arr), 100);
        for (int uli = 0; uli < event_num; ++uli)
        {
            int temp_sock = event_arr[uli].data.fd;
            if (pNet->_server_sock == temp_sock)
            {
                // ���ܿͻ������󣬲�ע�ᵽepoll
                int conn_sock = accept(pNet->_server_sock, NULL, 0);
                if (-1 == conn_sock)
                {
                    std::cout << "accept client socket fail errno=" << errno << std::endl;
                    continue;
                }

                std::cout << "accept client request conn_sock=" << conn_sock << std::endl;

#if 0
int keepAlive = 1;    
int keepIdle = 5;        //Ĭ��Ϊ7200��2Сʱ
int keepInterval = 5;    //Ĭ��75��
int keepCount = 1;       //Ĭ��Ϊ9��
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
        // ���¼�����
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
                // ���׽��ִ�epoll������ɾ����Ȼ���ڹر��׽���
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
                    // ���ݶ�ȡ���
//                    GThreadPoolManager::instance().push_msg("common_work", [&](){std::cout << "read over and read all data sock=" << event_sock << " data=" << read_data << std::endl;});
                    break;
                }
                else
                {
                    // ���ݶ�ȡ�쳣
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


