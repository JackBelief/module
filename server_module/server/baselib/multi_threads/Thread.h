#pragma once


#include <pthread.h>



#include "MsgQueue.h"

class Thread
{
    public:
        Thread(){}

        Thread(bool* pstop, MsgQueue* pmsg) : _is_stop(pstop), _msg_queue(pmsg), _is_running(false)
        {

        }

        ~Thread()
        {

        }

    public:
        bool start();
        bool join();

  void display(){std::cout << "111";}

    private:
        static void* run(void* arg);

    private:
        bool*                                                 _is_stop;         // 线程运行结束标记
        MsgQueue*                                             _msg_queue;       // 线程读取的消息队列
        bool                                                  _is_running;      // 记录当前线程的状态——是否在运行中
        pthread_t                                             _tid;             // 线程ID
};
