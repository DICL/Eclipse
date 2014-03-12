#ifndef __EXCEPTION_HH_
#define __EXCEPTION_HH_

#include <iostream>
#include <exception>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//UNIX dependent error libraries
#include <errno.h>
#include <error.h>
#include <execinfo.h>

extern char* program_invocation_short_name;

using std::string;
using std::ostream;

class Exception: public std::runtime_error {
 protected:
  char message [256]; // enought for log message 

 public:
  Exception () {}

  Exception (const char* file, const char* in) {
   snprintf (message, 256, "EXCEPTION:%s:%s ",
     program_invocation_short_name, file);
   extra_information (in);
  }

  Exception (const string& in) {
   snprintf (message, 128, "EXCEPTION: %s", in.c_str ());
  }

  Exception (const Exception& e) {
   strncpy (message, e.message, 256);
  }

  ~Exception() throw() {}

  const char* what () const throw() override {
   return message;
  }

  void extra_information (const char* in) {
   char tmp [128], hostname [64]; 

   gethostname (hostname, 64);
   snprintf (tmp, 128, "[ERRNO: %i] [STR: %s]"
     " [REASON: %s] [HOSTNAME: %s]",
     errno, strerror (errno), in, hostname);
   strncat (message, tmp, 256);
  }

  const char* backtrace () const {
   size_t size;
   void* trace [1<<6];
   char** strings;
   static char bt_output [1<<10];

   size = ::backtrace (trace, 1<<6);
   strings = backtrace_symbols (trace, 1<<6);

   strcpy (bt_output, "=====BACKTRACE=====\n");
   for (size_t i = 0; i < size; i++) {
    strncat (bt_output, strings[i], 1<<8);
    strncat (bt_output, "\n", 1<<8);
   }
   free (strings);

   return bt_output;
  }

  friend ostream& operator<< (ostream& in, Exception& e) {
   in << e.message;
   return in;
  }
};

#endif
