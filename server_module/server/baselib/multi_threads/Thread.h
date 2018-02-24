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
        bool*                                                 _is_stop;         // �߳����н������
        MsgQueue*                                             _msg_queue;       // �̶߳�ȡ����Ϣ����
        bool                                                  _is_running;      // ��¼��ǰ�̵߳�״̬�����Ƿ���������
        pthread_t                                             _tid;             // �߳�ID
};
