#pragma once

#include <assert.h>
#include <sys/time.h>
#include <semaphore.h>

// �ź����Ĵ���ͻ���������
class Semaphore
{
    public:
        Semaphore()
        {
            _psem = (sem_t*)malloc(sizeof(sem_t));         // ����ռ䣬�����ź���

            assert(NULL != _psem);
            assert(sem_init(_psem, 0, 0) == 0);            // �ź�����ʼ������ʼֵΪ0�������ڹ���(�β��б�(sem_t *__sem, int __pshared, unsigned int __value))
        }

        ~Semaphore()
        {
            if (_psem)
            {
                sem_destroy(_psem);                        // �ź�������

                free(_psem);                               // �ź����ռ��ͷ�
                _psem = NULL;
            }
        }

    public:
        bool wait()
        {
            // �����ȴ�ֱ�����ź�����ֵ��1
            return 0 == sem_wait(_psem);
        }

        bool post()
        {
            // ԭ�Ӳ������ź�����ֵ��1
            return 0 == sem_post(_psem);
        }

        bool timewait(int usecs = 1000000)
        {
            // ʱ�䵥λ��΢���ʱwait
            static const int time_convet_unit = 1000000;
            if (usecs <= 0)
            {
                usecs = time_convet_unit;
            }

            struct timeval tv;
            gettimeofday(&tv, NULL);

            struct timespec ts; 
            ts.tv_sec = tv.tv_sec + usecs / time_convet_unit;
            ts.tv_nsec = (tv.tv_usec + usecs) % time_convet_unit * 1000;
            return sem_timedwait(_psem, &ts) == 0;
        }

        int get_value()
        {
            if (_psem)
            {
                // ���ݳ�ʼ�����ź���ȡֵ���ڵ���0
                int sem_value = 0;
                if (0 == sem_getvalue(_psem, &sem_value) && sem_value >= 0)
                {
                    return sem_value;
                }
            }

            return -1;
        }

    private:
        sem_t*                                                _psem;           // �ź���ָ��
};

