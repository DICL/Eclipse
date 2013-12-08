#ifndef __TASK_HH_
#define __TASK_HH_

#include <string.h>
#include <stddef.h>

#ifndef PATH_LENGTH
#define PATH_LENGTH 128 
#endif 

class Task {
 public:
  Task (const char * path) {
   strncpy (this->path, path, PATH_LENGTH - 1);
   this->path [PATH_LENGTH - 1] = '\0';          //! Safe String
   this->length = PATH_LENGTH;
  }

  Task (const Task& that) {
   this->length = that.length;
   strncpy (this->path, that.path, PATH_LENGTH - 1);
  }

  ~Task () {}

  void get_path (char * path) {
   strncpy (path, this->path, PATH_LENGTH);
  } 

 protected:
  char path [PATH_LENGTH];
  size_t length;
};

#endif
