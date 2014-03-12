// AUTHOR: vicente.bolea@gmail.com
//
//
//
//
//

#ifndef __LOGGER_HH_
#define __LOGGER_HH_

#include <syslog.h>
#include <iostream>

#ifndef _LOG_NAME_
#define _LOG_NAME_ "MRR"
#endif 

#ifndef _LOG_TYPE_
#define _LOG_TYPE_ LOG_LOCAL0
#endif 

using std::istream;

class logger_t {
 public:
  logger_t () {};

  void operator() (const char * _in) {
   openlog (_LOG_NAME_, LOG_PID | LOG_CONS, _LOG_TYPE_);
   syslog (LOG_ERR, "%s", _in);
   closelog ();
  }
};

logger_t logger;

#endif
