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
#define CACHE_DEFAULT_SIZE (1 << 20)              //! 1 MiB
#endif

#include <SETcache.hh>
#include <DHTclient.hh>
#include <hash.hh>

#include <pthread.h>
#include <vector>
#include <iostream>
#include <vector>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdexcept>
#include <functional>

using std::vector;

// }}}
// Cache class {{{
// -------------------------------------------- * * * -- Vicente Bolea
//
class Cache {
 public:
  Cache ();
  Cache (size_t s, uint8_t = 0);
  ~Cache ();

  //size_t get_size ()          { return this->cache.get_size (); }
  void set_maxsize (size_t s) { 
    this->cache = new SETcache (s); 
  }
  int set_policy (int p)      { policies = p; return p; }
  void set_network (int, int, const char*, const char**, const char*);

  bool bind ();
  void run ();
  void close ();

  std::tuple<char*, size_t> lookup (const char*) throw (std::out_of_range);
  bool insert (const char*, const char*, size_t = 0);

 protected: //! Thread functions
  static void* tfunc_migration_server (void*);
  static void* tfunc_migration_client (void*);
  static void* tfunc_request (void*);
  bool request (const char*);

 protected:
  DHTclient* DHT_client;
  uint8_t policies;
  SETcache* cache;
  size_t _size;

  uint8_t local_no;
  in_addr_t local_ip;
  char local_ip_str [INET_ADDRSTRLEN];
  vector<struct sockaddr_in> network;
  struct sockaddr_in Arequest, Amigration_server;
  int Prequest, Pmigration;
  int Srequest, Smigration_server, Smigration_client;

  pthread_t tmigration_client, tmigration_server, trequest;
  pthread_barrier_t barrier_start;
  bool tclient_continue, tserver_continue, tserver_request_continue;
  const int number_threads = 3;

  std::unordered_map<const char*, uint64_t> stats;
};
#endif /* end of include guard: CACHE_XU5J91EC */
// }}}
