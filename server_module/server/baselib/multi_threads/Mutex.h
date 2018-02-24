#pragma once

#include <assert.h>
#include <pthread.h>


class Mutex
{
    public:
        Mutex()
        {
            // 因为底层的原因，此处最终选择还是使用malloc
            _pmutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));                  // 申请空间创建锁
            assert(NULL != _pmutex);

            // pthread_mutex_init和pthread_mutex_destroy配合完成锁资源的初始化和释放
            assert(0 == pthread_mutex_init(_pmutex, NULL));                               // 锁空间初始化
        }

        ~Mutex()
        {
            if (_pmutex)
            {
                pthread_mutex_destroy(_pmutex);                                           // 锁空间销毁

                free(_pmutex);
                _pmutex = NULL;
            }
        }

    public:
        bool lock()
        {
            return 0 == pthread_mutex_lock(_pmutex);
        }

        bool unlock()
        {
            return 0 == pthread_mutex_unlock(_pmutex);
        }

    private:
        pthread_mutex_t*                                       _pmutex;                   // 互斥锁指针，锁涉及到资源空间，所以会进行初始化和销毁处理
};


// 构造即加锁、析构即解锁
class RAIIMutex
{
    public:
        RAIIMutex(Mutex* p) : _pMutex(p)
        {
            if (NULL == _pMutex || false == _pMutex->lock())
            {
                _pMutex = NULL;
            } 
        }

        ~RAIIMutex()
        {
            if (NULL != _pMutex && true == _pMutex->unlock())
            {
                _pMutex = NULL;
            }
        }

    private:
        Mutex*                                                 _pMutex;                 // 互斥锁对象指针
};

