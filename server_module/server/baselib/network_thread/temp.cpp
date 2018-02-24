#include <unistd.h>
#include <signal.h>
#include "NetworkThread.h"

#include <iostream>
using namespace std;

void handle_int_signal(int signal_num)
{
    GNetworkThread::instance().stop();
//    GThreadPoolManager::instance().stop();    
    std::cout << "make all thread pools stop" << std::endl;
    return;
}

int main()
{
    signal(SIGINT, handle_int_signal);

    GNetworkThread::instance().init();
    GNetworkThread::instance().join();
    return 0;
}
