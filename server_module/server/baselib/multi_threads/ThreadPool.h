#pragma once


#include <memory>
#include <functional>
#include <vector>

#include "Thread.h"
#include "MsgQueue.h"

class ThreadPool
{
    public:
       ThreadPool(int count) : _thread_count(count), _is_stop(false)
       {

       }

       ~ThreadPool()
       {

       }

    public:
        bool start();
        bool stop();
        bool join();
        bool push_msg(const std::function<void(void)>& msg);

    private:
        int                                            _thread_count;               // �߳�����
        bool                                           _is_stop;                    // �������  ���Ƶ�ǰ�����̳߳��Ƿ�ر�
        MsgQueue                                       _msg_queue;                  // ��Ϣ����
        std::vector<std::shared_ptr<Thread> >          _thread_vec;                 // �߳�����
};



