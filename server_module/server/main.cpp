#include <signal.h>

#include <iostream>
using namespace std;


#include "baselib/network_thread/NetworkThread.h"
#include "baselib/multi_threads/ThreadPoolManager.h"


void handle_int_signal(int signal_num)
{
    GNetworkThread::instance().stop();
    GThreadPoolManager::instance().stop();    
    std::cout << "make all thread pools stop" << std::endl;
    return;
}

bool server_init()
{
    signal(SIGINT, handle_int_signal);
    GThreadPoolManager::instance().add_new_pool("common_work", 2);
    GThreadPoolManager::instance().add_new_pool("response_work", 2);

    return GThreadPoolManager::instance().start() && GNetworkThread::instance().init();
}

void server_join()
{
    GNetworkThread::instance().join();
    GThreadPoolManager::instance().join();
    return;
}

int main()
{
    if (!server_init())
    {
        std::cout << "multi or network thread init fail" << std::endl;
        return -1;
    }

    server_join();
    return 0;
}

