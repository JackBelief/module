
#include <functional>
#include <iostream>
using namespace std;

#include "Thread.h"


template <typename T>
void* thread_func(void* arg)
{
    T* pthis = (T*)arg;
    pthis->display();
    return NULL;
}

#if 0
template <>
void* thread_func(void* arg)
{
    Thread* pthis = (Thread*)arg;
    pthis->display();
    return NULL;
}
#endif

bool Thread::start()
{
    if (!_is_running)
    {
//        if (0 != pthread_create(&_tid, NULL, &Thread::run, this))
        if (0 != pthread_create(&_tid, NULL, thread_func<Thread>, this))
        {
            return false;
        }

        std::cout << "thread create success tid=" << _tid << std::endl;
        _is_running = true;
    }

    return true;
}

// 等待线程运行结束，回收其资源
bool Thread::join()
{
    if (_is_running)
    {
        pthread_join(_tid, NULL);
        _is_running = false;
    }

    return true;
}

void* Thread::run(void* arg)
{
    Thread* pThis = (Thread*)arg;
    std::function<void(void)> func;

    // 每个线程都是死循环，只有当线程池停止运行时，自己才会停止运行
    while (!*(pThis->_is_stop))
    {
        // 每个线程死循环的等待从线程池消息队列中取出元素来处理，但是，有个超时限制
        if (pThis->_msg_queue->pop_msg(func, 100))
        {
            func();
        }
    }

    std::cout << "thread stop running tid=" << pThis->_tid << std::endl;
    return NULL;
}


