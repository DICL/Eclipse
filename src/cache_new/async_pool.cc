#include "async_pool.hh"
#include <utility>      // std::pair, std::get

using namespace std;

// Constructor & destr {{{
Async_pool::Async_pool(int _max) : max_tasks(_max) { }
// - - -
Async_pool::~Async_pool() { }
// }}}
// schedule {{{
void Async_pool::schedule () {

  remove_if (running_tasks.begin(), running_tasks.end(), [](std::future f) {
    return task_.valid();
  });

  if (running_tasks.size() < max_tasks and not awaiting_tasks.empty()) {
    auto t = awaiting_tasks.pop_front();
    auto first  = std::get<0> (t);
    auto second = std::get<1> (t);
    auto third  = std::get<2> (t);
    auto fourth = std::get<3> (t);
    running_tasks.push_back(async(launch::async, &first, second, third, fourth));
  }
}
// }}}
// add_Task {{{
void async_add_task (Bundle bundle) {
  queue.push_back (bundle);
}
// }}}
