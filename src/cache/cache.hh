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

#define STATUS_VIRGIN     0x0
#define STATUS_LOADED     0x1
#define STATUS_READY      0x2
#define STATUS_RUNNING    0x3
#define STATUS_CLOSED     0x4

#define SETTED_IFACE      0x1     // 0 0 0 0 0 1
#define SETTED_HOST       0x2     // 0 0 0 0 1 0
#define SETTED_NETWORK    0x4     // 0 0 0 1 0 0 
#define SETTED_PORT       0x8     // 0 0 1 0 0 0 
#define SETTED_SIZE       0x10    // 0 1 0 0 0 0 
#define SETTED_POLICY     0x20    // 1 0 0 0 0 0 
#define SETTED_ALL       (0x40-1) // 1 1 1 1 1 1 

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
  ~Cache ();

  Cache& set_size    (size_t); 
  Cache& set_policy  (int);     
  Cache& set_port    (int);  
  Cache& set_iface   (const char*);
  Cache& set_host    (const char*);
  Cache& set_network (std::vector<const char*>);

  Cache& bind ();
  Cache& run ();
  Cache& close ();

  std::tuple<char*, size_t> lookup (const char*) throw (std::out_of_range);
  bool insert (const char*, const char*, size_t = 0);

 //--------------THREAD FUNCTION--------------------------------//
 protected: 
  void migration_server ();
  void migration_client ();
  void request ();
  bool request (const char*);

 protected:
  int status, setted, policies;
  DHTclient* DHT_client;
  SETcache* cache;
  size_t _size;

  //--------------NETWORKING MEMBERS-----------------------------//
  uint16_t local_no; in_addr_t local_ip; char local_ip_str [INET_ADDRSTRLEN];
  char host [INET_ADDRSTRLEN];
  vector<struct sockaddr_in> network;
  struct sockaddr_in Arequest, Amigration_server;
  int Prequest, Pmigration;
  int Srequest, Smigration_server, Smigration_client;

  //----------------THREADS THINGS-------------------------------//
  pthread_t tmigration_client, tmigration_server, trequest;
  pthread_barrier_t barrier_start;
  bool tclient_continue, tserver_continue, tserver_request_continue;
  const int number_threads = 3;

  std::unordered_map<const char*, uint64_t> stats;
};
#endif /* end of include guard: CACHE_XU5J91EC */
// }}}
