#ifndef __SCHEDULER_HH_
#define __SCHEDULER_HH_

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <utils.hh>
#include <address_book.hh>
#include <vector>
#include <task.hh>

using std::vector;

class IScheduler {
 public:
  virtual ~IScheduler ();
  virtual bool listen () = 0;
  virtual int upload (Task&) = 0;
  virtual Task& recv (char*) = 0;
};

class Scheduler: public IScheduler {
 // Attributes and getters/setters
 protected:
  Scheduler& set_signals ();
  Address_book* book;

 protected:
  virtual int select_slave (uint64_t key); 

 public:
  Scheduler () { } 
  virtual ~Scheduler () { delete book; } 
 
  void  set_network (int argc, const char ** argv);
  void set_port (int in) { book->set_port (in); }
  int get_port () { return book->get_port (); }

  virtual bool listen ();
  virtual int upload (Task&);
  virtual Task& recv (char* file_name);
};

#endif
