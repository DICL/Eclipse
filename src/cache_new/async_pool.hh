#pragma once

#include <future>
#include <queue>
#include <vector>
#include <string>
#include "node.hh"

class Cache_slave;

using Task_fun = std::function<void(Cache_slave*, std::string, Node&)>;
using Bundle = std::tuple<Task_fun, Cache_slave*, std::string, Node&>;

class Async_pool {
  public:
    Async_pool(int);
    ~Async_pool();

    void schedule ();
    void add_task (Bundle);

  private:
    std::queue<Bundle> awaiting_tasks;
    std::vector<std::future<void> > running_tasks;

    int max_tasks;
};
