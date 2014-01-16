// @brief 
//  This cache class aim to meet the requirements
//   - Fix size
//   - Thread Reentrant support
//   - Different discarding policies, such as: LRU, locality distance
//
// -------------------------------------------- * * * -- Vicente Bolea
//
// @usage: 
//  Cache my_cache (10);
//  my_cache.insert (01039595813, "Vicente's number");
//  cout << "Vicente's phone number" << cache.lookup (01039595813) << endl;
//
// -------------------------------------------- * * * -- Vicente Bolea
// Preprocessor {{{
#ifndef CACHE_XU5J91EC
#define CACHE_XU5J91EC

#define CACHE_LRU         0x0
#define CACHE_SPATIAL     0x1
#define CACHE_SYNCHRONIZE 0x2
#define CACHE_MIGRATION   0x4
#define CACHE_PUSH        0x8
#define CACHE_PULL        0x10

#ifndef CACHE_DEFAULT_SIZE
#define CACHE_DEFAULT_SIZE (2 << 20)              //! 1 MiB
#endif

#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <map>
#include <hash.h>

using std::map;

// }}}
// Cache class {{{
// -------------------------------------------- * * * -- Vicente Bolea
struct Chunk {
 char * str;
 size_t size;
 uint64_t time;
 uint64_t point;
};
// }}}
// Cache class {{{
// -------------------------------------------- * * * -- Vicente Bolea
//
class Cache {
 public:
  Cache (size_t s) { this->_size = s; }
  Cache (size_t s, uint8_t p = 0) { 
   this->_size = s; 
   this->policies = p; 
  }
  Cache () { this->_size = _DEFAULT_SIZE; }
  ~Cache ();

  void set_maxsize (size_t s) { this->_size = s; }
  size_t get_size ()          { return this->_map.size; }
  int set_policy (int p)      { policies = p; return p; }

  bool lookup (int, char*, size_t*);
  bool insert (int, char*, size_t);

 protected:
  void discard ();
  void discard_lru ();
  void discard_spatial ();

 protected:
  map<uint64_t, Chunk*> map_time;              //! int to string
  map<uint64_t, Chunk*> map_spatial;           //! int to string
  size_t _size;
  uint8_t policies;
};
#endif /* end of include guard: CACHE_XU5J91EC */
// }}}
