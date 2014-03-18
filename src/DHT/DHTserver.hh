//
// @brief Blocking server for distributed hash table 
//  
// :TODO: UDP Sockets may be more suitable ?
//
// Preprocessor {{{
#ifndef __DHTSERVER_HH__
#define __DHTSERVER_HH__

#include <utils.hh>  //! Where the hash function is
#include <hash.hh>
#include <macros.h>  //! Some macros
#include <string.h>
#include <stdio.h>  
#include <sys/time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <map>

using std::map;

// }}}
// DHTserver class definition {{{
class DHTserver {
 public:
  DHTserver (int port) {
   this->port = port;
   thread_continue = true;
  }
  ~DHTserver() { this->close (); };

  bool bind ();
  bool set_interface (const char*);
  bool listen ();
  bool close ();
  bool report (const char*, const char*);

 protected:
  bool server_request (int key);
  int server_receive ();
  static void* listening (void*);

 protected:
  int port;
  int server_fd;
  struct sockaddr_in server_addr; 

  pthread_t tserver;
  map<uint32_t, uint32_t> table; //! O (log n)
  bool thread_continue; //! there is not race condition
};

#endif
// }}}
