#include <address_book_page.hh>

Address_book_page& Address_book_page::accept (int sock) {
 socklen_t sin_size = sizeof (struct sockaddr_in);
 fd = ::accept (sock, (struct sockaddr *)&addr, &sin_size);
 status = CONNECTED;
 log (M_INFO, "SCHEDULER", "Backend server linked (addr = %s).", inet_ntoa(addr.sin_addr)); 

 return *this;
}

Address_book_page& Address_book_page::send (Packet* packet, bool trace = false) {
 //packet->set_time (time);
 packet->trace = trace;

 ::send_msg (fd, "QUERY");
 ::send (fd, packet, packet.get_size(), 0);

// if (trace)
//  log (M_DEBUG, "SCHEDULER", "[QUERY: %i] sent to backend node: %s", 
//       (int)point, inet_ntoa (addr.sin_addr)); 

 return *this;
}
