// Preprocessor {{{
#ifndef __SETCACHE_HH_
#define __SETCACHE_HH_

#include <packets.hh>

#include <set>
#include <queue>
#include <fstream>
#include <string>
#include <stdexcept>

using std::set;
using std::queue;
using std::ifstream;
using std::ostream;
using std::string;
using std::endl;
using std::cout;
using std::ios;
using std::ios_base;
// }}}

enum policy {
 NOTHING  = 0x0,
 UPDATE   = 0x1,
 LRU      = 0x2,
 SPATIAL  = 0x4,
 JOIN     = 0x8
};

class SETcache {
 protected:
  set<diskPage, bool (*) (const diskPage&, const diskPage&)>* cache;
  set<diskPage, bool (*) (const diskPage&, const diskPage&)>*
   cache_time;
  char path [256];
  int _max, policy;
  uint64_t count;
  double boundary_low, boundary_upp, ema;

  pthread_mutex_t mutex_match     ;
  pthread_mutex_t mutex_queue_low ;
  pthread_mutex_t mutex_queue_upp ;

  void pop_farthest ();

 public:
  SETcache (int, char * p = NULL);
  ~SETcache () { delete cache; delete cache_time;}

  void set_policy (int);
  void setDataFile (char*);
  bool match (Query&);
  bool is_valid (diskPage&);
  void update (double low, double upp);
  diskPage get_diskPage (uint64_t);
  diskPage lookup (uint64_t) throw (std::out_of_range);
  bool insert (diskPage&);

  diskPage get_low ();
  diskPage get_upp ();

  friend ostream& operator<< (ostream&, SETcache&);

  queue<diskPage> queue_lower;
  queue<diskPage> queue_upper;
};

#endif
