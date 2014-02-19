// Preprocessor {{{
#ifndef __SETCACHE_HH_
#define __SETCACHE_HH_

#include <packets.hh>
#include <diskpage.hh>

#include <set>
#include <queue>
#include <fstream>
#include <string>
#include <stdexcept>

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
}

class Local_cache {
 protected:
  typedef MAP map<uint64_t, std::pair<uint64_t, disk_page_t> >;
  MAP map_spatial, map_lru;
  int policy;
  size_t max_size, current_size, count;
  uint64_t double boundary_low, boundary_upp, ema;
  pthread_mutex_t mutex_map_spatial, mutex_map_lru;
  pthread_mutex_t mutex_queue_low, mutex_queue_upp;

  void pop_farthest ();

 public:
  Local_cache (size_t);
  ~Local_cache () { delete map_spatial; delete map_lru; }

  void set_policy (int);

  bool insert (disk_page_t&);
  disk_page_t lookup (uint64_t) throw (std::out_of_range);

  bool is_disk_page_belonging (disk_page_t&);
  void update (double low, double upp);

  disk_page_t get_low ();
  disk_page_t get_upp ();

  queue<disk_page_t> queue_lower;
  queue<disk_page_t> queue_upper;
};

#endif
