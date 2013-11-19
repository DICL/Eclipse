#ifndef __ADDRESS_BOOK_PAGE_HH__
#define __ADDRESS_BOOK_PAGE_HH__
//
// Includes {{{
#include <utils.hh>
#include <EWMA.hh>
#include <packets.hh>
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
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <cfloat>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
//
//}}}
//
enum Status { DISCONNECTED, CONNECTED, CLOSED, ERROR };
class IAddress_book_page {
 public:
  virtual IAddress_book_page& accept (int) = 0;
  virtual IAddress_book_page& send (Header&, bool) = 0;
  virtual IAddress_book_page& send_msg (const char *) = 0;
  virtual IAddress_book_page& close (void) = 0;
  virtual ~IAddress_book_page (void) = 0;
};

class Address_book_page: public IAddress_book_page {
 public:
  int fd;
  struct sockaddr_in addr;
  char host [64];
  Status status;

 public:
  Address_book_page (const char * host, int port) { 
   strncpy (this->host, host, 64);

   addr.sin_family = AF_INET; 
   addr.sin_port = htons (port);
   addr.sin_addr.s_addr = inet_addr (host);

   bzero (&(addr.sin_zero), 8);
   status = DISCONNECTED;
  }

  virtual ~Address_book_page () { this->close();}

  Address_book_page& set_fd (int f) { fd = f; return *this;}
  const int get_fd () const { return fd; }
  const Status get_status () const { return status; } 

  virtual Address_book_page& accept (int);
  virtual Address_book_page& send (Packet*, bool);
  virtual Address_book_page& send_msg (const char * in) { ::send_msg (fd, in); return *this; }
  virtual Address_book_page& close () { ::close (fd); status = CLOSED; return *this; }
};

#endif
