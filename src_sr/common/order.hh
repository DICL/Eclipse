#ifndef __ORDER_HH
#define __ORDER_HH

#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string>
#include <assert.h>
#include <list>

using std::list;
using std::string;

const  size_t DISK_PAGE_SIZE = 4092;

class Order {

 public:
  enum type {
    DOWNLOAD = 0x0,
    UPLOAD   = 0x1
  };

 protected:
  enum type purpose; 
  char file_name [128];
  list<uint8_t*> list_chunk; //! Splited in 4KiB chuncks

 public:
  Order () { }
  //Order (char*);
  Order (char*, uint8_t*, size_t);

  Order& set_file_name (const char* in) { 
   assert (in != NULL); 
   strncpy (file_name, in, 128); 
   return *this;
  }
  
  Order& set_data (uint8_t* data);
  const char* get_file_name () { return file_name; }

  void deserialize (uint8_t*);
  uint8_t* serialize (size_t*);
 
  //friend ostream& operator<< (ostream& os, Order o) {
  // return o.serialize ();
  //}

  //friend istream& operator>> (istream& is, Order o) {
  // return o.deserialize (is.);
  //}

}__attribute__((aligned));

#endif
