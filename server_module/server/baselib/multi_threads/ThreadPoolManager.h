#pragma once

#include <memory>
#include <unordered_map>

#include "ThreadPool.h"
#include "../common/Singleton.h"


class ThreadPoolManager
{
    public:
        ThreadPoolManager()
        {

        }

        ~ThreadPoolManager()        
        {

        }

    public:
        bool start();
        bool stop();
        bool join();
        bool add_new_pool(const std::string& pool_name, int thread_count = 8);
        bool push_msg(const std::string& pool_name, const std::function<void(void)>& msg);

    private:
        std::unordered_map<std::string, std::shared_ptr<ThreadPool>>     _thread_pool_mp;               // 线程池管理对象
};

typedef Singleton<ThreadPoolManager> GThreadPoolManager;


