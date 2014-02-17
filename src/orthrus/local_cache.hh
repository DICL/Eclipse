// Preprocessor {{{
#ifndef __SETCACHE_HH_
#define __SETCACHE_HH_

#include <packets.hh>

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

enum policy {
 NOTHING  = 0x0,
 UPDATE   = 0x1,
 LRU      = 0x2,
 SPATIAL  = 0x4,
 JOIN     = 0x8
};

class disk_page_t {
 public:
  disk_page_t ();
  ~disk_page_t ();
  disk_page_t (const disk_page_t& that) {
   memcpy (this->data, that.data, DPSIZE);
  }
  disk_page_t& operator (const disk_page_t& that) {
   memcpy (this->data, that.data, DPSIZE);
   return *this;
  }
  char data [DPSIZE];
};

class Local_cache {
 protected:
  std::map<uint64_t, std::pair<uint64_t, disk_page_t> > cache;
  int _max, policy;
  uint64_t count;
  double boundary_low, boundary_upp, ema;
  pthread_mutex_t mutex_cache, mutex_queue_low, mutex_queue_upp;

  void pop_farthest ();

 public:
  Local_cache (int);
  ~Local_cache () { delete cache; delete cache_time;}
  void set_policy (int);

  bool insert (diskPage&);
  diskPage lookup (uint64_t) throw (std::out_of_range);

  bool is_valid (diskPage&);
  void update (double low, double upp);
  diskPage get_diskPage (uint64_t);

  diskPage get_low ();
  diskPage get_upp ();

  queue<diskPage> queue_lower;
  queue<diskPage> queue_upper;
};

#endif
