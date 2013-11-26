#include <address_book.hh>

// Setup the socket (listen) {{{ 
// ----------------------------------------------------
Address_book& Master::listen () {
 int one = 1;

 addr.sin_family = AF_INET;
 addr.sin_port = htons (port);
 addr.sin_addr.s_addr = INADDR_ANY;
 bzero (&(addr.sin_zero), 8);
 status = DISCONNECTED;

 if ((sock = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  log (M_ERR, "SCHEDULER", "Socket");

 if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "SCHEDULER", "Setsockopt");

 if (bind (sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1)
  log (M_ERR, "SCHEDULER", "Unable to bind");

 if (::listen (sock, nslaves + 1) == -1)
  log (M_ERR, "SCHEDULER", "Listen");

 log (M_INFO, "SCHEDULER", "Network setted up using port = %i", port);
 
 return *this;
}

// }}}
// send {{{ 
// ----------------------------------------------------
Address_book& Address_book::send (File* f, bool trace = false) {
 packet->set_time (time);
 packet->trace = trace;

 ::send_msg (fd, "QUERY");
 ::send (fd, packet, packet.get_size(), 0);

 if (trace)
  log (M_DEBUG, "SCHEDULER", "[QUERY: %i] sent to backend node: %s", 
       (int)point, inet_ntoa (addr.sin_addr)); 

 return *this;
}

// }}}
