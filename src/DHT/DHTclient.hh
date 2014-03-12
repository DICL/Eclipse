//
// @brief Blocking client for distributed hash table 
//  
// :TODO: UDP Sockets may be more suitable ?
// :TODO: Which hash function use ?
//
// Preprocessor {{{
#ifndef __DHTSERVER_HH__
#define __DHTSERVER_HH__
// 
#include <DHTmacros.h>  //! Some macros
#include <utils.hh>  //! Where the hash function is
#include <hash.hh>

#include <unordered_map>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>


// }}}
// DHTclient class definition {{{
// ------------------------------------
class DHTclient {
 public:
  DHTclient (const char * ip, int port) {
   strncpy (this->ip, ip, INET_ADDRSTRLEN);
   this->port = port;
  }

  ~DHTclient () { close (); }
  
  int lookup (const char *); 
  char* lookup_str (const char *); 
  bool bind ();
  bool close ();
 
 protected:
  bool server_request (uint32_t key);
  int server_receive ();

 protected:
  char ip [INET_ADDRSTRLEN];
  int port;
  int server_fd, client_fd;
  struct sockaddr_in server_addr; 
  struct sockaddr_in client_addr; 
};

#endif
// }}}
