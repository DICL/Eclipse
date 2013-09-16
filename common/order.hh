#ifndef __ORDER_HH
#define __ORDER_HH

#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

class Order {
 private:
  size_t DISK_PAGE_SIZE;
  char file_name [128];
  list<uint8_t*> list_chunk; //! Splited in 4KiB chuncks

 public:
  Order (char*, void*, size_t);
  Order (char*);

  void deserialize (char*);
  uint8_t* serialize (size_t*);
 
  friend ostream& operator<< (ostream& os, Order o) {
   return o.serialize ();
  }

  //friend istream& operator>> (istream& is, Order o) {
  // return o.deserialize (is.);
  //}

}__attribute__((aligned));

#endif
