#pragma once

#include <assert.h>
#include <sys/time.h>
#include <semaphore.h>

// 信号量的处理和互斥锁类似
class Semaphore
{
    public:
        Semaphore()
        {
            _psem = (sem_t*)malloc(sizeof(sem_t));         // 申请空间，创建信号量

            assert(NULL != _psem);
            assert(sem_init(_psem, 0, 0) == 0);            // 信号量初始化，初始值为0，进程内共享(形参列表：(sem_t *__sem, int __pshared, unsigned int __value))
        }

        ~Semaphore()
        {
            if (_psem)
            {
                sem_destroy(_psem);                        // 信号量销毁

                free(_psem);                               // 信号量空间释放
                _psem = NULL;
            }
        }

    public:
        bool wait()
        {
            // 阻塞等待直到将信号量的值减1
            return 0 == sem_wait(_psem);
        }

        bool post()
        {
            // 原子操作将信号量的值加1
            return 0 == sem_post(_psem);
        }

        bool timewait(int usecs = 1000000)
        {
            // 时间单位是微妙，定时wait
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
                // 根据初始化，信号量取值大于等于0
                int sem_value = 0;
                if (0 == sem_getvalue(_psem, &sem_value) && sem_value >= 0)
                {
                    return sem_value;
                }
            }

            return -1;
        }

    private:
        sem_t*                                                _psem;           // 信号量指针
};

