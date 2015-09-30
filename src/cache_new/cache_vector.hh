#pragma once
#include "cache"

template <typename KEY, typename VALUE>
class Cache_vector: public Cache {
  using PAIR = std::pair<KEY,VALUE>
  private:
    vector<PAIR> container;

  public:
    Cache_vector(int);

    override VALUE& lookup(KEY);
    override bool exists(KEY);
    override void insert(KEY,VALUE);
};

// lookup {{{
template <typename KEY, typename VALUE>
override VALUE& Cache_vector::lookup(KEY) {
  for (auto& pair_ : container) {
    if (pair_.first == KEY) return pair_.second;
  }
  throw (std::out_of_range())
}
//}}}
// exists {{{
override bool Cache_vector::exists(KEY) {
  for (auto& pair_ : container) {
    if (pair_.first == KEY) return true;
  }
  return false;
}
//}}}
// insert {{{
override void Cache_vector::insert(KEY, VALUE) {
  if (exists (KEY)) {
    container.push_front();
  }
  container.push_back(make_pair (KEY, VALUE));
}
//}}}
