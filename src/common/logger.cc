// @file
// @author Vicente Adolfo Bolea Sanchez
// @brief Simple Logger implementation using syslog
//
#include "logger.hh"

#include <stdarg.h>
#include <syslog.h>
#include <unordered_map>

using std::string;

std::unordered_map<string, int> syslog_facilities {
  {"LOG_LOCAL1" , LOG_LOCAL1},
  {"LOG_LOCAL2" , LOG_LOCAL2},
  {"LOG_LOCAL3" , LOG_LOCAL3},
  {"LOG_LOCAL4" , LOG_LOCAL4},
  {"LOG_LOCAL5" , LOG_LOCAL5},
  {"LOG_LOCAL6" , LOG_LOCAL6},
  {"LOG_LOCAL7" , LOG_LOCAL7},
  {"LOG_DAEMON" , LOG_DAEMON},
  {"LOG_USER" , LOG_USER}
};

std::unordered_map<int, string> syslog_priorities {
  {LOG_INFO    , "LOG_INFO"},
  {LOG_ERR     , "LOG_ERR"},
  {LOG_WARNING , "LOG_WARNING"}
};

Logger* Logger::singleton = nullptr;

Logger* Logger::connect (string title, string type) {
  if (singleton == nullptr)
    singleton = new Logger(title, type);

  return singleton;
}

void Logger::disconnect (Logger* in) {
  if (singleton != nullptr) {
    delete singleton;
    singleton = nullptr;
  }
  in = nullptr;
}

Logger::Logger (string title, string type) { 
  int type_ = syslog_facilities[type];
  openlog (title.c_str() , LOG_CONS, type_); 
}

Logger::~Logger () { closelog (); }

void Logger::info (const char* fmt, ...) { 
  va_list ap;

  va_start(ap, fmt);
  log(LOG_INFO, fmt, ap);
  va_end(ap);
}

void Logger::warn (const char* fmt, ...) { 
  va_list ap;

  va_start(ap, fmt);
  log(LOG_WARNING, fmt, ap);
  va_end(ap);
}

void Logger::error (const char* fmt, ...) { 
  va_list ap;

  va_start(ap, fmt);
  log(LOG_ERR, fmt, ap);
  va_end(ap);
}

void Logger::log (int type, const char* fmt, va_list ap) { 
  vsyslog (type, fmt, ap);
}
