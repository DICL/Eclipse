// AUTHOR: vicente.bolea@gmail.com
//
//
//
//
//

#ifndef __LOGGER_HH_
#define __LOGGER_HH_

const char *error_str [32] = {
 "\e[31mERROR\e[0m",   //! RED COLOR
 "\e[35mWARN\e[0m",    //! MAGENTA COLOR 
 "\e[32mDEBUG\e[0m",   //! GREEN COLOR 
 "\e[34mINFO\e[0m"     //! BLUE COLOR
};

const char *error_str_nocolor [20] = {"[ERROR]", "[WARN]", "[DEBUG]", "[INFO]"};


class logger {
 protected:
  static void log (int type, const char* in, ...) {
    va_list args;
    char host [32];

    get_host (host);
    if (isatty (fileno (stdout)) || true)
     fprintf (stderr, "%s\e[33m::\e[0m[\e[36m%s\e[0m]\e[1m \e[33m", error_str [type], host);
    else 
     fprintf (stderr, "%s::[%s] ", error_str_nocolor [type], host);

    va_start (args, in);
    vfprintf (stderr, in, args);
    va_end (args);

    if (isatty (fileno (stdout)) || true)
     fprintf (stderr, "\e[0m\n");
    else 
     fprintf (stderr, "\n");

    if (type == M_ERR) exit (EXIT_SUCCESS);
   }

 public:
  //logger ();
  virtual ~logger ();

  // @override
  virtual static void get_host (char* host) {
   strncpy (host, "UNDETERMINED", 32);
  }

  // @override
  virtual static void set_color () { return true; }

  static void info (const char* in, ...) {
    va_list args;
    va_start (args, in);
    log (3, in, args);
    va_end (args);
  }

  static void debug(const char* in, ...) {
    va_list args;
    va_start (args, in);
    log (2, in, args);
    va_end (args);
  }

  static void warn(const char* in, ...) {
    va_list args;
    va_start (args, in);
    log (1, in, args);
    va_end (args);
  }

  static void err(const char* in, ...) {
    va_list args;
    va_start (args, in);
    log (0, in, args);
    va_end (args);
  }
};

#endif