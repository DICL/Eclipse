#include <address_book_page.hh>
//
// Address_book_page {{{
//
Address_book_page::Address_book_page (const char * host, int port) { 
 strncpy (this->host, host, 64);

 addr.sin_family = AF_INET; 
 addr.sin_port = htons (port);
 addr.sin_addr.s_addr = inet_addr (host);

 bzero (&(addr.sin_zero), 8);
 status = DISCONNECTED;
}

// }}}
// Accept {{{
//
Address_book_page& Address_book_page::accept (int sock) {
 socklen_t sin_size = sizeof (struct sockaddr_in);
 fd = ::accept (sock, (struct sockaddr *)&addr, &sin_size);
 status = CONNECTED;
 log (M_INFO, "SCHEDULER", "Backend server linked (addr = %s).", inet_ntoa(addr.sin_addr)); 

 return *this;
}

// }}}
// send {{{
//
Address_book_page& Address_book_page::send (
 Packet* packet,     //! The initialized Packet to be sent
 bool trace = false  //! Check whether you want to track the packet
) {

 //packet->set_time (time);
  //packet->trace = trace;

 ::send_msg (fd, "QUERY");
 ::send (fd, packet, packet->get_size(), 0);

 // if (trace)
 //  log (M_DEBUG, "SCHEDULER", "[QUERY: %i] sent to backend node: %s", 
 //       (int)point, inet_ntoa (addr.sin_addr)); 

 return *this;
}

// }}}
// send_msg {{{
//
Address_book_page& Address_book_page::send_msg (const char * in) {
 ::send_msg (fd, in); 
 return *this; 
}

Address_book_page& Address_book_page::close () { 
 ::close (fd); 
 status = CLOSED; 
 return *this; 
}
// }}}
