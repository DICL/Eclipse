#include <DHTclient.hh>

// h {{{
// ----------------------------------------------- 
#ifndef __H_FUNCTION__
#define __H_FUNCTION__

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32_t h (const char * data, size_t len) {
 uint32_t hash = len, tmp;
 int rem;

 if (len <= 0 || data == NULL) return 0;

 rem = len & 3;
 len >>= 2;

 /* Main loop */
 for (;len > 0; len--) {
  hash  += get16bits (data);
  tmp    = (get16bits (data+2) << 11) ^ hash;
  hash   = (hash << 16) ^ tmp;
  data  += 2*sizeof (uint16_t);
  hash  += hash >> 11;
 }

 /* Handle end cases */
 switch (rem) {
  case 3: hash += get16bits (data);
          hash ^= hash << 16;
          hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
          hash += hash >> 11;
          break;
  case 2: hash += get16bits (data);
          hash ^= hash << 11;
          hash += hash >> 17;
          break;
  case 1: hash += (signed char)*data;
          hash ^= hash << 10;
          hash += hash >> 1;
 }

 /* Force "avalanching" of final 127 bits */
 hash ^= hash << 3;
 hash += hash >> 5;
 hash ^= hash << 4;
 hash += hash >> 17;
 hash ^= hash << 25;
 hash += hash >> 6;

 return hash;
}
#endif
// }}}
// lookup {{{
//
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
