#ifndef __NODE_CLIENT_HH_
#define __NODE_CLIENT_HH_

#include <utils.hh>
#include <packets.hh>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <cfloat>
#include <string.h>
#include <stdlib.h>
#include <iostream>

class Node { 
	protected:
  //Packet next_packet;

		double EMA, low_b, upp_b, alpha;
		int fd;
		uint64_t time;
		struct sockaddr_in addr;

	public:
		Node () : EMA (.0), low_b (.0), upp_b (.0), alpha (.0) {} 
		Node (double a) : EMA (.0), low_b (.0), upp_b (.0), alpha (a) {} 
		Node (double a, double e) : EMA (e), low_b (.0), upp_b (.0) , alpha (a) {} 

		Node& set_fd (int f)        { fd = f;    return *this;}
		Node& set_EMA (double a)    { EMA= a;    return *this;}
		Node& set_alpha (double a)  { alpha = a; return *this;}
		Node& set_low (double l)    { low_b = l; return *this;}
		Node& set_upp (double u)    { upp_b = u; return *this;} 
		Node& set_time (uint64_t u) { time = u;  return *this;} 

		int get_fd () { return fd; }
		double get_low () { return low_b; }
		double get_upp () { return upp_b; } 
		double get_EMA () const { return EMA; }

		double get_distance (Packet& p) { return fabs (EMA - p.get_point ()); }
		double get_distance (uint64_t p) { return fabs (EMA - p); }

		Node& update_EMA (double point)  { EMA += alpha * (point - EMA); return *this; } 

		Node& accept (int sock) {
			socklen_t sin_size = sizeof (struct sockaddr_in);
			fd = ::accept (sock, (struct sockaddr *)&addr, &sin_size);
			log (M_INFO, "SCHEDULER", "Backend server linked (addr = %s).", inet_ntoa (addr.sin_addr)); 
			return *this;
		}

		Node& send (uint64_t point, bool trace = false) {
			Packet toSend;
   toSend .set_point (point) .set_EMA (EMA) .set_low (low_b) 
          .set_upp (upp_b);
   toSend. set_time (time);

   toSend.trace = trace;
			::send_msg (fd, "QUERY");
			::send (fd, &toSend, sizeof (Packet), 0);

   if (trace)
    log (M_DEBUG, "SCHEDULER", "[QUERY: %i] sent to backend node: %s", 
         (int)point, inet_ntoa (addr.sin_addr)); 

			return *this;
		}

		Node& send_msg (const char * in) { ::send_msg (fd, in); return *this; }
		Node& close () { ::close (fd); return *this; }
};

#endif
