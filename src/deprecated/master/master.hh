#ifndef __SR_MASTER_HH_
#define __SR_MASTER_HH_

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <packetfactory.hh>
#include <utils.hh>
#include <order.hh>
#include <address_book.hh>
#include <vector>

using std::vector;

class IMaster {
 public:
  virtual ~IMaster ();
  virtual bool listen () = 0;
  virtual int upload (Order&) = 0;
  virtual Order& recv (char*) = 0;
};

class Master: public IMaster {
 // Attributes and getters/setters
 protected:
  Master& set_signals ();

 protected:
  virtual int select_slave (uint64_t key); 

 public:
  Master () { } 
  virtual ~Master () { }

  virtual int upload (Order&);
  virtual Order& recv (char* file_name);
};

#endif