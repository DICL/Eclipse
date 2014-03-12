#include <dht.hh>
#include <assert.h>

DHT::DHT () {}
DHT::DHT (int port, int n, const char* ifa, const char ** in) { 
 set_network (port, n, ifa, in); 
}

DHT::~DHT () {
 ::close (sock); 
 for (int i = 0; i < _nservers; i++)
  delete network_ip [i];

 delete network_ip;
 delete network_addr;
}

void DHT::set_network (int port, int n, const char* ifa, const char ** in) {
 assert (in != NULL);

 if (ifa == NULL) {
  ifa = const_cast<const char*> (strdup ("eth0"));
 }

 this->port = port;
 _nservers = n;
 network_ip = new char* [_nservers];  
 network_addr = new struct sockaddr_in [_nservers];

 strcpy (local_ip, get_ip (ifa));
 sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);

 for (int i = 0; i < _nservers; i++) {
  network_ip [i] = new char [32];
  strcpy (network_ip[i], in[i]);
 }

 //! Get local index
 local_no = 0;
 while (local_no < _nservers) {
  if (strncmp (network_ip [local_no], local_ip, 32) == 0) break;
  local_no++;
 }

 //! Fill address vector
 for (int i = 0; i < _nservers; i++) {
  network_addr [i] .sin_family      = AF_INET;
  network_addr [i] .sin_port        = htons (this->port);
  network_addr [i] .sin_addr.s_addr = inet_addr (network_ip [i]);
  bzero (&(network_addr [i].sin_zero), 8);
 }
}

bool DHT::check (Header& h) {
 assert (&h != NULL);
 return (local_no == (int)(h.get_point () / 100000)) ? true : false;
}

bool DHT::request (Header& h) {
 assert (&h != NULL);

 socklen_t s = sizeof (struct sockaddr);
 int server_no = h.get_point ()/ 100000;

 if (h.trace)
  log (M_DEBUG, local_ip, "Requesting: %i", (int)h.get_point());

 if (server_no == local_no) return false; 
 sendto (sock, &h, sizeof (Header), 0,(sockaddr*)&network_addr [server_no], s);

 if (h.trace)
  log (M_DEBUG, local_ip, "Request sent waiting for response from %s",
       network_ip [server_no]);

 return true;
}
