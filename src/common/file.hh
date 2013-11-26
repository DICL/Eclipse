#ifndef __FILE_HH__
#define __FILE_HH__

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
//
//
class File {
 
 public:
  File (const char * _name) {
   strncpy (filename, _name, 32);
  } 

  ~File () { }

//  void serialize (); 
  size_t get_size () { return size; }
  const char* get_filename () { return filename; }

  uint8_t* data;

 protected:
  char filename [32];
  size_t size; 
};
//
//
#endif 
