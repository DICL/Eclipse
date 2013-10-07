#include <master.hh>

// Hash function {{{ 
// ----------------------------------------------------
int h (const char* k, size_t length = 0) {
 uint8_t* seed = (uint8_t*) &k;
 uint32_t _key = 0;

 if (!length) length = 4;

 for (size_t i = 0; i < sizeof(char) % 5; i++)
  _key += (uint32_t) (seed[i] << (0x8 * i));

 return _key % length;
}

// }}}
// Setup the socket (listen) {{{ 
// ----------------------------------------------------
bool Master::listen () {
 int one = 1;
 struct sockaddr_in addr;

 addr.sin_family = AF_INET;
 addr.sin_port = htons (port);
 addr.sin_addr.s_addr = INADDR_ANY;
 bzero (&(addr.sin_zero), 8);

 if ((sock = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  log (M_ERR, "SCHEDULER", "Socket");

 if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "SCHEDULER", "Setsockopt");

 if (bind (sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1)
  log (M_ERR, "SCHEDULER", "Unable to bind");

 if (::listen (sock, nslaves + 1) == -1)
  log (M_ERR, "SCHEDULER", "Listen");

 log (M_INFO, "SCHEDULER", "Network setted up using port = %i", port);
 
 return true;
} 

//}}}
// Sample RR algorithm {{{
// ----------------------------------------------------
int Master::select_slave (uint64_t key) {
 static int i = 0;
 return (i++ % nslaves);
}

//}}}
// upload {{{
// ----------------------------------------------------
int Master::upload (Order& o) {
 const char* file_name  = o.get_file_name ();
 int slave_victim = select_slave (h (file_name, strlen (file_name)));
 assert (slaves[slave_victim].get_status() == Address_book::CONNECTED);
 Packet* packet = PacketFactory::fromOrder (o);

 //! Forward the data to one of the slaves
 slaves [slave_victim] .send (packet);

 return slave_victim;
} 
//}}}
