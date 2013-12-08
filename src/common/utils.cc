#include <utils.hh>

#include <execinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <stdint.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <cfloat>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

const char *error_str [20] = {
 "[\e[31mERROR\e[0m]",   //! RED COLOR
 "[\e[35mWARN\e[0m]",    //! MAGENTA COLOR 
 "[\e[32mDEBUG\e[0m]",   //! GREEN COLOR 
 "[\e[34mINFO\e[0m]"     //! BLUE COLOR
};

const char *error_str_nocolor [20] = {"[ERROR]", "[WARN]", "[DEBUG]", "[INFO]"};


void 
log (int type, const char* _ip, const char* in, ...) 
{
 va_list args;

 if (isatty (fileno (stdout)) || true)
   fprintf (stderr, "%s\e[33m::\e[0m[\e[36m%s\e[0m]\e[1m \e[33m", error_str [type],  _ip);
 else 
   fprintf (stderr, "%s::[%s] ", error_str_nocolor [type],  _ip);

 va_start (args, in);
 vfprintf (stderr, in, args);
 va_end (args);

 if (isatty (fileno (stdout)) || true)
   fprintf (stderr, "\e[0m\n");
 else 
   fprintf (stderr, "\n");

 if (type == M_ERR) exit (EXIT_SUCCESS);
}

inline bool
fd_is_ready (int fd) 
{
 struct timeval timeout = {1, 0};

 fd_set readSet;
 FD_ZERO(&readSet);
 FD_SET(fd, &readSet);

 if ((select(fd+1, &readSet, NULL, NULL, &timeout) >= 0) && 
     FD_ISSET(fd, &readSet))
  return true;

 else 
  return false;
}

uint64_t
timediff (struct timeval *end_time, struct timeval *start_time)
{
 return  (end_time->tv_usec + (1000000 * end_time->tv_sec)) 
  - (start_time->tv_usec + (1000000 * start_time->tv_sec));
}

 void
send_msg (int socket, const char* send_data)
{
 int msg_len = strlen (send_data);
 send (socket, &msg_len, sizeof(int), 0); 
 send (socket, send_data, msg_len, 0); 
}

 void
recv_msg (int socket, char* recv_data)
{
 int nbytes, bytes_received = 0, r = 0;
 while (r != 4)
  r += recv (socket, (char*) &nbytes+r, sizeof(int)-r, MSG_WAITALL);

 // read nbytes;
 while (bytes_received < nbytes)
  bytes_received += recv (socket, recv_data+bytes_received, nbytes-bytes_received, MSG_WAITALL);

 recv_data [bytes_received] = 0;
}

 int
poisson (double c)
{ 
 int x = 0;
 srand (time(NULL));

 for (double t = .0; t <= 1.0; x++) {
  t -= log ((double)(rand()%1000) / 1000.0) / c;
 }
 return x;
}

//rotate/flip a quadrant appropriately
 void
rot (int64_t n, int64_t  *x, int64_t *y, int64_t  rx, int64_t ry) 
{
 if (ry == 0) {
  if (rx == 1) {
   *x = n - 1 - *x;
   *y = n - 1 - *y;
  }

  //Swap x and y
  int64_t t  = *x;
  *x = *y;
  *y = t;
 }
}

 int64_t
hilbert (int64_t n, int64_t x, int64_t y) 
{
 int64_t rx, ry, s, d = 0;
 for (s = n / 2; s > 0; s /= 2) {
  rx = (x & s) > 0;
  ry = (y & s) > 0;
  d += s * s * ((3 * rx) ^ ry);
  rot (s, &x, &y, rx, ry);
 }
 return d;
}

uint64_t prepare_input (char* in) {
 int64_t a, b, ret;
 sscanf (in, "%" SCNi64 " %" SCNi64 , &a , &b );
 a /= 2000;
 b /= 2000;
 ret = hilbert (1024, a, b);
 return ret;
}

char* get_ip (const char* interface) {
 static char if_ip [INET_ADDRSTRLEN];
 struct ifaddrs *ifAddrStruct = NULL, *ifa = NULL;

 getifaddrs (&ifAddrStruct);

 for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
  if (ifa->ifa_addr->sa_family == AF_INET && strcmp (ifa->ifa_name, interface) == 0)
   inet_ntop (AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, 
     if_ip, INET_ADDRSTRLEN);

 if (ifAddrStruct != NULL) freeifaddrs (ifAddrStruct);

 return if_ip;
}

void dump_trace (void) {
	void * buffer[255];
	const int calls = backtrace (buffer, sizeof(buffer) / sizeof(void *));
	backtrace_symbols_fd (buffer, calls, 2);
}
