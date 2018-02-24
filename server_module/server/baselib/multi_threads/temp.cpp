#include <iostream>
#include <functional>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "ThreadPoolManager.h"

void global_display(const std::string& str)
{
    std::cout << str << std::endl;
}

class Student
{
    public:
        Student()
        {
            std::cout << "Student Con" << std::endl;
        }

        ~Student()
        {
            std::cout << "Student Des" << std::endl;
        }
};

class Display
{
    public:
        Display(){number = 120;}
        ~Display(){}

        void func_dis()
        {
            static Student stu;
        }

        void nostatic_display(const std::string& str)
        {
            std::cout << str << std::endl;
        }

        static void static_display(const std::string& str)
        {
            std::cout << str << "tid = " << pthread_self() << " number = " << number << std::endl;
        }
    private:
        static int number;
};
int Display::number = 10;

void* func(void* arg)
{
    for (int uli = 0; uli < 10; ++uli)
    {
        GThreadPoolManager::instance().push_msg("common_work", std::bind(Display::static_display, "tianqingyang" + std::to_string(uli)));
    }

    return NULL;
}

void handle_int_signal(int signal_num)
{
    GThreadPoolManager::instance().stop();
    std::cout << "make all thread pools stop" << std::endl;
    return;
}

int main()
{
    signal(SIGINT, handle_int_signal);

    GThreadPoolManager::instance().add_new_pool("common_work", 2);
    GThreadPoolManager::instance().start();

    for (int uli = 0; uli < 10; ++uli)
    {
        GThreadPoolManager::instance().push_msg("common_work", std::bind(Display::static_display, "tqy" + std::to_string(uli)));
    }

    pthread_t tid;
    pthread_create(&tid, NULL, func, NULL);
    pthread_join(tid, NULL);

    GThreadPoolManager::instance().join();

#if 0
    ThreadPool pool(1);
    pool.start();
    std::cout << "game over" << std::endl;

    pool.push_msg(std::bind(Display::static_display, "tqy"));

    pool.join();
    std::cout << "game over" << std::endl;
#endif
    return 0;
}




