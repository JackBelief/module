#include "ThreadPool.h"



bool ThreadPool::start()
{
    for (int uli = 0; uli < _thread_count; ++uli)
    {
        // �����̶߳���ÿ���̹߳����̳߳ص���Ϣ���кͽ������
        _thread_vec.emplace_back(std::make_shared<Thread>(&_is_stop, &_msg_queue));
        _thread_vec[uli]->start();
    }

    return true;
}

bool ThreadPool::stop()
{
    _is_stop = true;
    return true;
}

bool ThreadPool::join()
{
    for (auto& element : _thread_vec)
    {
        element->join();
    }

    return true;
}

bool ThreadPool::push_msg(const std::function<void(void)>& msg)
{
    return _msg_queue.push_msg(msg);
}




