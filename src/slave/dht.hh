#ifndef __DHT_HH_
#define __DHT_HH_

#include <macros.h>
#include <packets.hh>
#include <utils.hh>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

const uint64_t data_set = 1000000;

class DHT {
 public:
  DHT ();
  DHT (int, int, const char*, const char**);
  ~DHT ();

  void set_network (int, int, const char*, const char**);
  bool check (Header&);
  bool request (Header&);

 protected:
  int sock, port;
  char ** network_ip;
  struct sockaddr_in *network_addr;

  char local_ip [32];
  int local_no;
  int _nservers;
};

#endif
