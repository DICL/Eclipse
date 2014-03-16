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
#include <vector>

#include <Address_book_page.hh>

//}}}

using std::vector;

class Address_book { 
 protected:
  int fd, nslaves, sock;
  struct sockaddr_in addr;
  vector<Address_book_page> array;

 public:
  Address_book (int argc, const char ** argv) {
   nslaves = argc;
   for (int i = 0; i < argc; i++) {
    array [i] = Address_book_page (argv [i], port);
   }
  }
  virtual ~Address_book () { this->close();}

 public:
  Master& set_port (int);
  Address_book& set_fd (int f) { fd = f; return *this;}
  Master& set_nslaves (int);

  int get_port ()    { return port;}
  int get_nslaves () { return nslaves;}
  const int get_fd () const { return fd; }

 public:
  Address_book& listen ();
  Address_book& send (Packet&, bool);
  Address_book& send_msg (const char * in) { ::send_msg (fd, in); return *this; }
  Address_book& close () { ::close (fd); status = CLOSED; return *this; }
};

#endif
