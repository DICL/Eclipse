#ifndef __NODE_SKETCH_HH_
#define __NODE_SKETCH_HH_

#include <dht.hh>
#include <simring.hh>

#include <iostream>
#include <inttypes.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <error.h>

using namespace std;

class Node {
 protected:
  SETcache cache; 
  DHT dht;
  int sock_scheduler, sock_left, sock_right, sock_server;  
  int sch_port, peer_port, dht_port;
  char *local_ip, peer_left [32], peer_right[32], host_str [128], data_file [128];
  bool panic;

  uint32_t queryRecieves;
  uint32_t queryProcessed;
  uint64_t hitCount;
  uint64_t missCount;
  uint64_t TotalExecTime;
  uint64_t TotalWaitTime;

  const char * network_ip [10] = 
  {
   "192.168.1.1",
   "192.168.1.2",
   "192.168.1.3",
   "192.168.1.4",
   "192.168.1.5",
   "192.168.1.6",
   "192.168.1.7",
   "192.168.1.8",
   "192.168.1.9",
   "192.168.1.10"
  };

  pthread_t thread_neighbor;
  pthread_t thread_scheduler;
  pthread_t thread_forward;
  pthread_t thread_dht;

  struct sockaddr_in addr_left, addr_right, addr_server;

  static void* thread_func_dht       (void*) WEAK;
  static void* thread_func_scheduler (void*) WEAK;
  static void* thread_func_neighbor  (void*) WEAK;
  static void* thread_func_forward   (void*) WEAK;

  void setup_server_peer      (int, int*, struct sockaddr_in*) WEAK;
  void setup_client_peer      (const int, const char*, int*, struct sockaddr_in*) WEAK;
  void setup_client_scheduler (int, const char*, int*) WEAK;
  void parse_args             (int, const char**, Arguments*) WEAK;
  void close_all              (void) WEAK;
  void catch_signal           (int);

 public:
  Node (int, const char**, const char*, const char**);
  ~Node ();

  bool run ();
  bool join ();
  static void signal_handler ();

  static Node& instance;
};

#endif
