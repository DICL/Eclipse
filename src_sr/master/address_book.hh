// Preprocessor {{{
#ifndef __ADDRESS_BOOK_HH
#define __ADDRESS_BOOK_HH

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
//}}}

class IAddress_book {
 public:
  virtual IAddress_book& accept (int) = 0;
  virtual IAddress_book& send (Header&, bool) = 0;
  virtual IAddress_book& send_msg (const char *) = 0;
  virtual IAddress_book& close (void) = 0;
  virtual ~IAddress_book (void) = 0;
};

class Address_book: public IAddress_book { 
 public:
  enum Status { DISCONNECTED, CONNECTED, CLOSED, ERROR };

 protected:
  int fd;
  struct sockaddr_in addr;
  char host [64];
  Status status;

 public:
  Address_book (const char * host, int port) { 
   strncpy (this->host, host, 64);

   addr.sin_family = AF_INET; 
   addr.sin_port = htons (port);
   addr.sin_addr.s_addr = inet_addr (host);
   bzero (&(addr.sin_zero), 8);
   status = DISCONNECTED;
  }

  virtual ~Address_book () { this->close();}

  Address_book& set_fd (int f) { fd = f; return *this;}
  const int get_fd () const { return fd; }
  const Status get_status () const { return status; } 

  virtual Address_book& accept (int);
  virtual Address_book& send (Packet&, bool);
  virtual Address_book& send_msg (const char * in) { ::send_msg (fd, in); return *this; }
  virtual Address_book& close () { ::close (fd); status = CLOSED; return *this; }
};

#endif
