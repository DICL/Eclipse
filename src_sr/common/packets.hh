#ifndef __PACKETS_HH_
#define __PACKETS_HH_

#include <macros.h>
#include <utils.hh>
#include <stdint.h>
#include <cfloat>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>

class Header {
 public:
  uint64_t point;  //! Key/ index
  double EMA, low_b, upp_b;
  bool trace;

  Header () { trace = false; }
  Header& operator= (const Header& that) {
   point = that.point;
   EMA   = that.EMA;
   low_b = that.low_b;
   upp_b = that.upp_b;
   trace = that.trace;
   return *this;
  }

  Header& set_trazable ()        { trace = true; return *this; }
  Header& set_EMA (double e)     { EMA = e;      return *this; }
  Header& set_point (uint64_t p) { point = p;    return *this; }
  Header& set_low (double l)     { low_b = l;    return *this; }
  Header& set_upp (double u)     { upp_b = u;    return *this; }

  double   get_low   () { return low_b; }
  double   get_upp   () { return upp_b; }
  double   get_EMA   () { return EMA; }
  uint64_t get_point () { return point; }

  bool operator== (const Header& that) {
   return point == that.point ? true: false;
  }
}__attribute__((aligned));


class diskPage : public Header {
 public:
  uint64_t time;
  char chunk [DPSIZE];

  diskPage () : Header () {}
  diskPage (const uint64_t i) : Header () { point = i; }

  diskPage (const diskPage& that) : Header (that) {
   time = that.time;
   memcpy (chunk, that.chunk, DPSIZE);
  }

  diskPage& operator= (const diskPage& that) {
   Header::operator= (that);
   time = that.time;
   memcpy (chunk, that.chunk, DPSIZE);
   return *this;
  }

  static bool less_than (const diskPage& a, const diskPage& b) {
   return (a.point < b.point);
  }

  static bool less_than_lru (const diskPage& a, const diskPage& b) {
   return (a.time < b.time);
  }
}__attribute__((aligned));


/** @brief Class which represent an abstract Packet which
 * will be send by sockets
 */
class Packet: public Header {
 public:
  uint64_t time; 

  using Header::set_point;

  Packet () : Header () {} 
  Packet (uint64_t p) : Header () { point = p; }	
  Packet (const Packet& that) : Header (that), time (that.time) {}
  Packet& operator= (const Packet& that) {
   Header::operator= (that);
   time = that.time;
   return *this;
  }

  Packet& set_time (uint64_t t) { time = t; return *this; }
} __attribute__((aligned));

class Query: public Packet {
 protected:
  struct timeval scheduledDate;
  struct timeval startDate;
  struct timeval finishedDate;

 public:
  //constructor & destructor
  Query (): Packet() {}
  Query (const Packet&);
  Query (const Query&);

  //setter
  void setScheduledDate ();
  void setStartDate ();
  void setFinishedDate ();

  //getter
  uint64_t getWaitTime ();
  uint64_t getExecTime ();
}__attribute__((aligned));

#endif
