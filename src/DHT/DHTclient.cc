#include <DHTclient.hh>

// lookup {{{
// ----------------------------------------------- 
int DHTclient::lookup (const char * key) {
 //prepare packet
 uint32_t key_int = h (key, strlen (key));

 // Send it 
 server_request (key_int);

 // Receive back  
 int ip = server_receive ();

 return ip;
}
// }}}
// bind {{{
// ----------------------------------------------- 
bool DHTclient::bind () {
 // Setup client
 int one = 1;

 server_addr.sin_family = AF_INET;
 server_addr.sin_port = htons (port);
 server_addr.sin_addr.s_addr = inet_addr (ip);
 bzero (&(server_addr.sin_zero), 8);
 // status = DISCONNECTED;

 if ((server_fd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  log (M_ERR, "DHT", "Socket");

 if (setsockopt (server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "DHT", "Setsockopt");

 client_addr.sin_family = AF_INET;
 client_addr.sin_port = htons (port);
 client_addr.sin_addr.s_addr = inet_addr (ip);
 bzero (&(client_addr.sin_zero), 8);

 if ((client_fd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  log (M_ERR, "DHT", "Socket");

 if (setsockopt (client_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "DHT", "Setsockopt");

 if (::bind (client_fd, (struct sockaddr *)&client_addr, 
    sizeof(struct sockaddr)) == -1)
  log (M_ERR, "DHT", "Unable to bind");

 if (::listen (client_fd, 1) == -1)
  log (M_ERR, "DHT", "Listen");

 log (M_INFO, "DHT", "Network setted up using port = %i", port);

 return true;
}
// }}}
// server_request {{{
// ----------------------------------------------- 
bool DHTclient::server_request (uint32_t key) {
 /* code */
 int ret = sendto (server_fd, &key, 4, 0,          // 4 bytes for int
   (struct sockaddr*)&server_addr,
   sizeof (server_addr));

 switch (ret) {
  case -1: 
   return false;

  default:
   return true;
 } 
}
// }}}
// server_receive {{{
// ----------------------------------------------- 
int DHTclient::server_receive () {
 if (fd_is_ready (client_fd)) { 
  uint32_t reply;
  socklen_t sl = sizeof (client_addr);
  int ret = recvfrom (client_fd, &reply, 4 ,0, (struct sockaddr*)&client_addr, &sl);

  switch (ret) {
   case -1: 
    return false;

   default:
    return true;
  } 
 } 
 return false;
}
// }}}
// close {{{
// ----------------------------------------------- 
bool DHTclient::close () {
 ::close (server_fd);
 ::close (client_fd);
 return true;
}
// }}}
