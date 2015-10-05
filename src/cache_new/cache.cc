#include "cache.hh"

Cache::Cache(size_t s) : max_(s) {}

size_t Cache::get_max () const { return max_; }
Cache& Cache::set_max (size_t s) { max_ = s; return *this; }

