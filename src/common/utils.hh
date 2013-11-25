#ifndef __UTILS_HH_
#define __UTILS_HH_

#include <macros.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

using std::cerr;
using std::cout;
using std::endl;

enum m_error {
 M_ERR   = 0,
 M_WARN  = 1,
 M_DEBUG = 2,
 M_INFO  = 3
};

void        log           (int, const char* _ip, const char* in, ...);
inline bool fd_is_ready   (int);
uint64_t    timediff      (struct timeval*, struct timeval*);
void        send_msg      (int, const char*);
void        recv_msg      (int, char*) __attribute__((weak));
int         poisson       (double);
int64_t     hilbert       (int64_t n, int64_t x, int64_t y);
uint64_t    prepare_input (char* in);
char*       get_ip        (const char*);
void        dump_trace    (void);

#endif
