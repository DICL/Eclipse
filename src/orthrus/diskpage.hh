#ifndef DISKPAGE_52MS5HAW
#define DISKPAGE_52MS5HAW

#include <string.h>
#include <string>
#include <stddef>
#include <inttypes.h>
#include <stdint.h>

class disk_page_t {
 public:
  disk_page_t ();
  disk_page_t (const char * in) { deserialize (in); }
  disk_page_t (const disk_page_t& that) { *this = that; }
  disk_page_t& operator= (const disk_page_t& that) {
   index = that.index;
   time  = that.time;
   memcpy (this->data, that.data, DPSIZE);
   return *this;
  }
  ~disk_page_t ();

  char* serialize () {
   static char serialized [DPSIZE + 1024];
   char metadata [1024];
   snprintf (metadata, 1024, "INDEX:%32s:TIME:%32s", index, time);
   memcpy (serialized, metadata, 1024);
   memcpy (serialized + 1024, data, DPSIZE);
   return *this;
  }
  
  void deserialize (const char *in) {
   char metadata [1024];
   memcpy (metadata, in, 1024);
   memcpy (data, in + 1024, DPSIZE);

   if (strcmp (strtok (metadata, ":"), "INDEX"))  
    index = strtol (strtok (NULL, ":"), NULL, 10);
   else 
    throw std::string ("ConnectionException");

   if (strcmp (strtok (NULL, ":"), "TIME"))  
    time  = strtol (strtok (NULL, ":"), NULL, 10);
   else 
    throw std::string ("ConnectionException");

   return *this;
  }
  uint64_t index, time;
  char data [DPSIZE];
};

#endif /* end of include guard: DISKPAGE_52MS5HAW */
