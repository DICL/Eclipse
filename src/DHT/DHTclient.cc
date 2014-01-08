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

 if ((server_fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  log (M_ERR, "DHT", "Socket");

 if (setsockopt (server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "DHT", "Setsockopt");

#ifdef _DEBUG
 log (M_DEBUG, "DHTclient", "UDP client setted up host = [%s:%i]", 
      inet_ntoa(server_addr.sin_addr),
      ntohs (server_addr.sin_port));
#endif

 client_addr.sin_family = AF_INET;
 client_addr.sin_port = htons (port + 1);
 client_addr.sin_addr.s_addr = inet_addr (ip);
 bzero (&(client_addr.sin_zero), 8);

 if ((client_fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  log (M_ERR, "DHT", "Socket");

 if (setsockopt (client_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "DHT", "Setsockopt");

 if (::bind (client_fd, (struct sockaddr *)&client_addr, 
    sizeof(struct sockaddr)) == -1)
  log (M_ERR, "DHT", "Unable to bind");

 return true;
}
// }}}
// server_request {{{
// ----------------------------------------------- 
bool DHTclient::server_request (uint32_t key) {
 int key_serialized = htonl (key);
 int ret = sendto (server_fd, &key_serialized, 4, MSG_WAITALL,          // 4 bytes for int
   (struct sockaddr*)&server_addr, sizeof (server_addr));

#ifdef _DEBUG
 log (M_DEBUG, "DHTclient", "petition sent to %s:%i", inet_ntoa(server_addr.sin_addr),
     ntohs (server_addr.sin_port));
#endif

 switch (ret) {
  case -1: 
   return false;

  case 4:
   return true;
  default:
   perror ("Terrible error");
   return false;
 } 
}
// }}}
// server_receive {{{
// ----------------------------------------------- 
int DHTclient::server_receive () {
 //if (fd_is_ready (client_fd)) { 
  uint32_t reply = 0;
  socklen_t sl = sizeof (client_addr);
  int ret = recvfrom (server_fd, &reply, 4, MSG_WAITALL, (struct sockaddr*)&server_addr, &sl);

#ifdef _DEBUG
 //printf("Recieved %i\n", ntohl (reply));
#endif 

  switch (ret) {
   case -1: 
    return -1;

   default:
    return ntohl (reply);
  } 
 //} 
 return -1;
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
