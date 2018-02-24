#include <iostream>
using namespace std;

#include "ThreadPoolManager.h"


bool ThreadPoolManager::add_new_pool(const std::string& pool_name, int thread_count)
{
    if (thread_count <= 0 || pool_name.empty() || _thread_pool_mp.end() != _thread_pool_mp.find(pool_name))
    {
        std::cout << "add new pool fail name=" << pool_name << " t_count=" << thread_count << std::endl;
        return false;
    }

    std::shared_ptr<ThreadPool> thread_pool(new ThreadPool(thread_count));
    if (!thread_pool)
    {
        return false;
    }

    _thread_pool_mp[pool_name] = thread_pool;
    return true;
}

bool ThreadPoolManager::start()
{
    for (auto& element : _thread_pool_mp)
    {
        element.second->start();
    }

    return true;
}

bool ThreadPoolManager::join()
{
    for (auto& element : _thread_pool_mp)
    {
        element.second->join();
    }

    return true;
}

bool ThreadPoolManager::stop()
{
    for (auto& element : _thread_pool_mp)
    {
        element.second->stop();
    }

    return true;
}

bool ThreadPoolManager::push_msg(const std::string& pool_name, const std::function<void(void)>& msg)
{
    auto iter = _thread_pool_mp.find(pool_name);
    if (_thread_pool_mp.end() == iter)
    {
        std::cout << "push msg fail, no pool name=" << pool_name << std::endl;
        return false;
    }

    return iter->second->push_msg(msg);
}


