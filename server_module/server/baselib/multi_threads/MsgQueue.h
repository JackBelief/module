#pragma once

#include <list>
#include <functional>
#include <iostream>
using namespace std;

#include "Mutex.h"
#include "Semaphore.h"

const int msg_queue_size = 8;

class MsgQueue
{
    public:
        MsgQueue() : _queue_size(msg_queue_size)
        {

        }

        ~MsgQueue()
        {
            clear();
        }

    public:
        bool push_msg(const std::function<void(void)>& msg)
        {
            // 为了确保消息不要超出队列上限，因此，先加锁处理
            RAIIMutex temp_mutex(&_mutex);
            int sem_value = _sem.get_value();                    // 获取当前消息队列中消息元素的个数
            if (sem_value >= 0 && sem_value < _queue_size)
            {
                _queue.emplace_back(msg);                        // 消息入队尾
                _sem.post();                                     // 信号量加1，表示接收到一个消息
                return true;
            }

            std::cout << "push msg fail val=" << sem_value << std::endl;
            return false;
        }

        bool pop_msg(std::function<void(void)>& msg, int usecs)
        {
            // 对信号量设置超时等待
            if (!_sem.timewait(usecs))
            {
                return false;
            }

            // 如果先加锁后信号量处理存在死锁的风险
            RAIIMutex temp_mutex(&_mutex);
            msg = _queue.front();                              // 消息出队首
            _queue.pop_front();                                // 删除队首元素

            return true;
        }

        bool clear()
        {
            // 消息队列清空——删除每个消息元素
            RAIIMutex temp_mutex(&_mutex);
            while (_sem.get_value() > 0 && _sem.wait())
            {
                _queue.pop_front();
            }

            return true;
        }

        bool reset_queue_size(int new_size)
        {
            // 非频繁改动，暂不加锁
            _queue_size = new_size;
            return true;
        }

    private:
        int                                                            _queue_size;          // 队列大小
        Mutex                                                          _mutex;               // 互斥锁
        Semaphore                                                      _sem;                 // 信号量
        std::list<std::function<void(void)>>                           _queue;               // 消息队列(FIFO)
};


