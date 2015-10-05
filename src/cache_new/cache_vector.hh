#pragma once
#include "cache.hh"

template <typename KEY, typename VALUE>
class Cache_vector: public Cache<KEY,VALUE> {
  using PAIR = std::pair<KEY,VALUE>;
  private:
    vector<PAIR> container;

  public:
    Cache_vector(int);

    VALUE& lookup(KEY) override ;
    bool exists(KEY) override;
    void insert(KEY,VALUE) override;
};

// lookup {{{
template <typename KEY, typename VALUE>
VALUE& Cache_vector::lookup(KEY) {
  for (auto& pair_ : container) {
    if (pair_.first == KEY) return pair_.second;
  }
  throw (std::out_of_range())
}
//}}}
// exists {{{
template <typename KEY, typename VALUE>
bool Cache_vector::exists(KEY) {
  for (auto& pair_ : container) {
    if (pair_.first == KEY) return true;
  }
  return false;
}
//}}}
// insert {{{
template <typename KEY, typename VALUE>
void Cache_vector::insert(KEY, VALUE) {
  if (exists (KEY)) {
    container.push_front();
  }
  container.push_back(make_pair (KEY, VALUE));
}
//}}}
