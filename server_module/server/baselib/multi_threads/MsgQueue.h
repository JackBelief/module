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
            // Ϊ��ȷ����Ϣ��Ҫ�����������ޣ���ˣ��ȼ�������
            RAIIMutex temp_mutex(&_mutex);
            int sem_value = _sem.get_value();                    // ��ȡ��ǰ��Ϣ��������ϢԪ�صĸ���
            if (sem_value >= 0 && sem_value < _queue_size)
            {
                _queue.emplace_back(msg);                        // ��Ϣ���β
                _sem.post();                                     // �ź�����1����ʾ���յ�һ����Ϣ
                return true;
            }

            std::cout << "push msg fail val=" << sem_value << std::endl;
            return false;
        }

        bool pop_msg(std::function<void(void)>& msg, int usecs)
        {
            // ���ź������ó�ʱ�ȴ�
            if (!_sem.timewait(usecs))
            {
                return false;
            }

            // ����ȼ������ź���������������ķ���
            RAIIMutex temp_mutex(&_mutex);
            msg = _queue.front();                              // ��Ϣ������
            _queue.pop_front();                                // ɾ������Ԫ��

            return true;
        }

        bool clear()
        {
            // ��Ϣ������ա���ɾ��ÿ����ϢԪ��
            RAIIMutex temp_mutex(&_mutex);
            while (_sem.get_value() > 0 && _sem.wait())
            {
                _queue.pop_front();
            }

            return true;
        }

        bool reset_queue_size(int new_size)
        {
            // ��Ƶ���Ķ����ݲ�����
            _queue_size = new_size;
            return true;
        }

    private:
        int                                                            _queue_size;          // ���д�С
        Mutex                                                          _mutex;               // ������
        Semaphore                                                      _sem;                 // �ź���
        std::list<std::function<void(void)>>                           _queue;               // ��Ϣ����(FIFO)
};


