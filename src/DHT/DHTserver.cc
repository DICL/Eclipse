#include <errno.h>
#include <DHTserver.hh>

// DHTserver::report {{{
// ----------------------------------------------- 
bool DHTserver::report (const char * key, int server) {
 table[h(key, strlen (key))] = server; 
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
// status = DISCONNECTED;

 if ((server_fd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  log (M_ERR, "DHT", "socket function");

 if (setsockopt (server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "DHT", "setsockopt function");

 if (::bind (server_fd, (struct sockaddr*)&server_addr, 
     sizeof (server_addr)) == -1)
  log (M_ERR, "DHT", "bind function");

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

  int client_fd = accept4 (_this->server_fd, (struct sockaddr*)&(_this->server_addr), &sa, SOCK_NONBLOCK);
  if (client_fd == -1 || errno == EAGAIN) continue;                  //! Skip if no connection is presented

  int ret = recvfrom (client_fd, &input, 4, MSG_DONTWAIT, (struct sockaddr*)&client_addr, &sa); 

  switch (ret) {
   case -1: log (M_ERR, "DHT", "Error from recvfrom");        break; //! Error handle log
   case 0:  log (M_INFO, "DHT", "Shutdown message received"); break; //! Handle shutdown
   default:                                                          //! Handle normal situation
    uint32_t server_number = _this->table [input];
    sendto (client_fd, &server_number, 4, 0, (struct sockaddr*)&client_addr, sa);
    break; 
  }

 } while (_this->thread_continue);

 pthread_exit (NULL);
}
// }}}
