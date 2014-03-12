#ifndef DISKPAGE_52MS5HAW
#define DISKPAGE_52MS5HAW

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <string.h>
#include <string>
#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>

class disk_page_t {
 public:
  disk_page_t () {}
  disk_page_t (const char * in) { deserialize (in); }
  disk_page_t (const disk_page_t& that) { *this = that; }
  disk_page_t& operator= (const disk_page_t& that) {
   index = that.index;
   time  = that.time;
   size  = that.size;
   memcpy (this->data, that.data, size);
   return *this;
  }
  ~disk_page_t () { delete this->data; }
   
  uint64_t get_index () { return index; }
  uint64_t get_time ()  { return time; } 
  size_t   get_size ()  { return size; } 
  char*    get_data ()  { return data; } 

  disk_page_t& set_index (uint64_t i) { index = i; return *this; }
  disk_page_t& set_time  (uint64_t i) { time  = i; return *this; }
  disk_page_t& set_size  (size_t i)   { size = i; return *this; }
  disk_page_t& set_data  (const char* in) { 
   if (size > 0) {
    if (data == NULL) data = new char [size];
    strncpy (data, in, size);
   }
   return *this; 
  }

  char* serialize () {
   char *serialized = new char [size + 1024];
   char metadata [1024];
   snprintf (metadata, 1024,
    "INDEX:%032" PRIu64 ":TIME:%032" PRIu64 ":SIZE:%032zu", index, time, size);
   memcpy (serialized, metadata, 1024);
   if (data != NULL)
    memcpy (serialized + 1024, data, size);

   return serialized;
  }
  
  disk_page_t& deserialize (const char *in) {
   char metadata [1024];
   memcpy (metadata, in, 1024);

   if (strcmp (strtok (metadata, ":"), "INDEX") == 0)  //   || The head should
    index = strtol (strtok (NULL, ":"), NULL, 10);     //   || follow this scheme
   else                                                //   || It should not return
    throw std::string ("ConnectionException");         //   || an exception
                                                       //   ||
   if (strcmp (strtok (NULL, ":"), "TIME") == 0)       //   ||
    time  = strtol (strtok (NULL, ":"), NULL, 10);     //   ||
   else                                                //   ||
    throw std::string ("ConnectionException");         //   ||  
                                                       //   ||
   if (strcmp (strtok (NULL, ":"), "SIZE") == 0)       //   ||
    size  = strtol (strtok (NULL, ":"), NULL, 10);     //   ||
   else                                                //  \||/
    throw std::string ("ConnectionException");         //   \/

   data = new char [size]; 
   memcpy (data, in + 1024, size);
   return *this;
  }

 protected:
  uint64_t index, time;
  size_t size;
  char* data;
};

#endif /* end of include guard: DISKPAGE_52MS5HAW */
