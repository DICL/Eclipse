#pragma once

#include <orthrus/dataentry.hh>
#include <orthrus/datablock.hh>

class cache {
  public:
    cache();
    cache(size_t capacity);
    bool loookup(string filename);

  private:
    size_t curr_size_;
    size_t capacity_;
};

cache::cache() {
  curr_size_ = 0;
  capacity_ = CACHESIZE;
}

cache::cache(size_t num) {
  curr_size_ = 0;
  capacity_ = num;
}

bool cache::lookup(string filename) {

}
