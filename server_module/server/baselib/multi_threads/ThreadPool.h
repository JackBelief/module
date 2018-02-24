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
        int                                            _thread_count;               // 线程数量
        bool                                           _is_stop;                    // 结束标记  控制当前整个线程池是否关闭
        MsgQueue                                       _msg_queue;                  // 消息队列
        std::vector<std::shared_ptr<Thread> >          _thread_vec;                 // 线程容器
};



