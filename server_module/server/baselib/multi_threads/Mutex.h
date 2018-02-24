#pragma once

#include <assert.h>
#include <pthread.h>


class Mutex
{
    public:
        Mutex()
        {
            // ��Ϊ�ײ��ԭ�򣬴˴�����ѡ����ʹ��malloc
            _pmutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));                  // ����ռ䴴����
            assert(NULL != _pmutex);

            // pthread_mutex_init��pthread_mutex_destroy����������Դ�ĳ�ʼ�����ͷ�
            assert(0 == pthread_mutex_init(_pmutex, NULL));                               // ���ռ��ʼ��
        }

        ~Mutex()
        {
            if (_pmutex)
            {
                pthread_mutex_destroy(_pmutex);                                           // ���ռ�����

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
        pthread_mutex_t*                                       _pmutex;                   // ������ָ�룬���漰����Դ�ռ䣬���Ի���г�ʼ�������ٴ���
};


// ���켴����������������
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
        Mutex*                                                 _pMutex;                 // ����������ָ��
};

