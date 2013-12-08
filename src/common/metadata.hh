#ifndef __METADATA_HH_
#define __METADATA_HH_

#include <macros.h>
#include <utils.hh>
#include <stdint.h>
#include <cfloat>
#include <stdlib.h>
#include <string.h>

class Metadata {
 protected:
  uint64_t index, low_b, upp_b;
  double EMA;
  bool trace; 

 public:
  Metadata () {
   trace = false;
  }

  virtual ~Metadata () {}

  Metadata& set_trazable ()        { trace = true; return *this; }
  Metadata& set_EMA (double e)     { EMA = e;      return *this; }
  Metadata& set_index (uint64_t p) { index = p;    return *this; }
  Metadata& set_low (uint64_t l)   { low_b = l;    return *this; }
  Metadata& set_upp (uint64_t u)   { upp_b = u;    return *this; }

  uint64_t get_low   () { return low_b; }
  uint64_t get_upp   () { return upp_b; }
  double   get_EMA   () { return EMA; }
  uint64_t get_index () { return index; }

  bool operator== (const Metadata& that) {
   return index == that.index? true: false;
  }

  size_t get_size () {
   return sizeof (*this);
  }
}__attribute__((aligned));

#endif 
