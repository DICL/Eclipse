// Preprocessor {{{
#ifndef __SETCACHE_HH_
#define __SETCACHE_HH_

#include <packets.hh>
#include <diskpage.hh>

#include <map>
#include <queue>
#include <fstream>
#include <string>
#include <stdexcept>
#include <utility>

using std::map;
using std::queue;
using std::ifstream;
using std::ostream;
using std::string;
using std::endl;
using std::cout;
using std::ios;
using std::ios_base;
using std::make_pair;
using std::pair;
// }}}

namespace orthrus {
 enum {
  NOTHING  = 0x0,
  UPDATE   = 0x1,
  LRU      = 0x2,
  SPATIAL  = 0x4,
  JOIN     = 0x8
 };
};

class Local_cache {
 protected:
  typedef map<uint64_t, disk_page_t> MAP;

 public:
  Local_cache ();
  ~Local_cache ();

  //----------GETTERS & SETTERS-------------------//
  Local_cache& set_policy (int);
  Local_cache& set_boundaries (uint64_t, uint64_t);
  Local_cache& set_ema (uint64_t);
  Local_cache& set_size (size_t);
  int       get_policy ()     { return policy; }
  std::pair<uint64_t, uint64_t> get_boundaries () { 
   return make_pair (boundary_low, boundary_upp);
  }
  uint64_t  get_ema ()        { return ema; }
  size_t    get_size ()       { return size_max_bytes; }

  //----------PUBLIC METHODS----------------------//
  bool insert (uint64_t, disk_page_t&);
  disk_page_t lookup (uint64_t) throw (std::out_of_range);

  bool is_disk_page_belonging (disk_page_t&);
  uint64_t get_local_center ();
  void boundaries_update (uint64_t, uint64_t);
  disk_page_t get_upp ();
  disk_page_t get_low ();

  queue<disk_page_t> queue_lower;
  queue<disk_page_t> queue_upper;

 protected:
  MAP *map_spatial, *map_lru;
  int policy;
  size_t size_max_bytes, size_current_bytes, count;
  uint64_t boundary_low, boundary_upp, ema;
  pthread_mutex_t mutex_map_spatial, mutex_map_lru;
  pthread_mutex_t mutex_queue_low, mutex_queue_upp;

  void pop_farthest ();
};

#endif