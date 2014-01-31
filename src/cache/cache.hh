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
  Cache (size_t s, uint8_t);
  ~Cache ();

  //size_t get_size ()          { return this->cache.get_size (); }
  //void set_maxsize (size_t s) { this->cache. = s; }
  int set_policy (int p)      { policies = p; return p; }
  void set_network (int, int, const char*, const char**);

  bool bind ();
  void run ();
  void close ();

  std::tuple<char*, size_t> lookup (int) throw (std::out_of_range);
  bool insert (int, char*, size_t);

 protected: //! Thread functions
  static void* tfunc_server (void*);
  static void* tfunc_server_request (void*);
  static void* tfunc_client (void*);
  bool request (Header&);

 protected:
  uint8_t policies;
  SETcache* cache;

  in_addr_t local_ip;
  vector<struct sockaddr_in> network;
  struct sockaddr_in client_addr, server_addr;

  int local_no;
  size_t _size;
  int server_fd, port_dht, port_server;
  int sock_request, sock_scheduler, sock_left, sock_right, sock_server;  

  pthread_t tclient, tserver, tserver_request;
  pthread_barrier_t barrier_start;
  bool tclient_continue, tserver_continue, tserver_request_continue;
  std::function<void(const char*)> log_error, log_debug, log_warn;
};
#endif /* end of include guard: CACHE_XU5J91EC */
// }}}
