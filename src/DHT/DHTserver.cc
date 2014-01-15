#include <errno.h>
#include <DHTserver.hh>
#include <stdexcept>

// DHTserver::report {{{
// ----------------------------------------------- 
bool DHTserver::report (const char * key, const char * server) {
 table [h(key, strlen (key))] = inet_addr (server); 
 return true;
}
// }}}
// DHTserver::bind {{{
// ----------------------------------------------- 
bool DHTserver::bind () {
 int one = 1;

 bzero (&(server_addr.sin_zero), 8);
 server_addr.sin_family = AF_INET;
 server_addr.sin_port = htons (port);
 server_addr.sin_addr.s_addr = htonl (INADDR_ANY);

 if ((server_fd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  log (M_ERR, "DHT", "socket function");

 if (setsockopt (server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "DHT", "setsockopt function");

 if (::bind (server_fd, (struct sockaddr*)&server_addr, sizeof (server_addr)) == -1)
  log (M_ERR, "DHT", "bind function");

#ifdef _DEBUG
 log (M_INFO, "DHTserver", "UDP server setted up [%s:%i]", 
   inet_ntoa (server_addr.sin_addr), ntohs (server_addr.sin_port));
#endif

 return true;
}
// }}}
// DHTserver::set_interface {{{
// :XXX:     It is supposed to need root access
// :REQUEST: Root access
//                                -- Vicente Bolea
// -----------------------------------------------
bool DHTserver::set_interface (const char * _if) {
 setsockopt (server_fd, SOL_SOCKET, SO_BINDTODEVICE, _if, strlen (_if));
 return true;
}
// }}}
// DHTserver::listen {{{
// ----------------------------------------------- 
bool DHTserver::listen () {
 
 int ret = pthread_create (&(this->tserver), NULL, &DHTserver::listening, this);
 switch (ret) {
  case 0: return true;
  default: return false;
 }
}
/// }}}
// DHTserver::close {{{
// ----------------------------------------------- 
bool DHTserver::close () {
 thread_continue = false;
 int ret = pthread_join (tserver, NULL);
 ::close (server_fd);

 switch (ret) {
  case 0: return true;
  default: return false;
 }
 return true;
}
// }}}
// DHTserver::listening {{{
// :REQUEST: A lot of time
// :TODO:    Implement select function "fd_is_ready"
// ----------------------------------------------- 
void* DHTserver::listening (void* in) {
 uint32_t input;
 DHTserver* _this = (DHTserver*) in;
 socklen_t sa = sizeof (_this->server_addr);
 //================================================

 //! receiving petitions and sending back
 do {
  struct sockaddr_in client_addr;
  int ret = recvfrom (_this->server_fd, &input, 4, MSG_DONTWAIT, (struct sockaddr*)&client_addr, &sa); 

  if (ret == -1 && errno == EAGAIN) continue;                        //! No data received

  switch (ret) {
   case -1: log (M_DEBUG, "DHT", "Error from recvfrom");      break; //! Error handle log
   case 0:  log (M_INFO, "DHT", "Shutdown message received"); break; //! Handle shutdown
   default:                                                          //! Handle normal situation
            uint32_t server_number;
            try {
             server_number = htonl (_this->table.at (ntohl(input)));

            } catch (std::out_of_range& e) {
             server_number = htonl (DHT_NOT_FOUND);
#ifdef _DEBUG
             log (M_DEBUG, "DHT", "Key not found");
#endif
            }

#ifdef _DEBUG
            log (M_DEBUG, "DHTserver", "petition %u recieved -> %u", ntohl(input), ntohl (server_number));
#endif
            sendto (_this->server_fd, &server_number, 4, MSG_WAITALL, (struct sockaddr*)&client_addr, sa);
            break; 
  }

 } while (_this->thread_continue != false);

 pthread_exit (NULL);
}
// }}}
