#include <cache.hh>

// Constructors {{{
// ----------------------------------------------- 
Cache::Cache () { this->_size = _DEFAULT_SIZE; }

Cache::Cache (size_t s, uint8_t p = 0) { 
 this->cache = new SETcache (s);
 this->_size = s; 
 this->policies = p; 
}

Cache::~Cache () {
 ::close (sock_server); 
 ::close (sock_scheduler); 
 ::close (sock_left);
 ::close (sock_right);

 for (int i = 0; i < _nservers; i++)
  delete network_ip [i];

 delete network_ip;
 delete network_addr;
 delete cache;
}
// }}}
// bind {{{
// ----------------------------------------------- 
bool Cache::bind () {
 int one = 1;

 bzero (&(server_addr.sin_zero), 8);
 server_addr.sin_family = AF_INET;
 server_addr.sin_port = htons (port);
 server_addr.sin_addr.s_addr = htonl (INADDR_ANY);

 if ((server_fd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  log (M_ERR, "CACHEserver", "socket function");

 if (setsockopt (server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "CACHEserver", "setsockopt function");

 if (::bind (server_fd, (struct sockaddr*)&server_addr, sizeof (server_addr)) == -1)
  log (M_ERR, "CACHEserver", "bind function");

#ifdef _DEBUG
 log (M_INFO, "CACHEserver", "UDP server setted up [%s:%i]", 
   inet_ntoa (server_addr.sin_addr), ntohs (server_addr.sin_port));
#endif

 return true;
}
// }}}
// set_network {{{
// ----------------------------------------------- 
void Cache::set_network (int port, int n, const char* ifa, const char ** in) {
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
//}}}
// run {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::run () {
 pthread_create (&tserver, NULL, tfunc_server, this);
 pthread_create (&tserver, NULL, tfunc_client, this);
}
// }}}
// close {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::close () {
 pthread_join (tclient, NULL);
 pthread_join (tserver, NULL);
}
// }}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
//bool Cache::lookup (int key, char* output, size_t* s) {
// try {
//  Chunk* out = this->_map [key]; 
//
// } catch (std::out_of_range& e) {
//  return false;
// }
//
// memcpy (output, out->str, out->size);
// *s = out->second;
//
// return false;
//}
// }}}
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Cache::insert (int key, char* output, size_t s) {
 diskPage dp (key);
 memcpy (dp.chunk, output, s);
 bool ret = cache.match (dp);

 // Code to ask DHT :TODO:
 if (ret == false) {
  int victim = DHTclient_i.lookup (output);
  if (victim != _this->local_no) {
   //Do some routine to get that chunk

  }
 }

 return ret;
}
// }}}
// tfunc_server {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_server (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t sa = sizeof (_this->server_addr);
 //================================================
 do {
  diskPage dp;
  if (fd_is_ready (sock_server)) {

   int ret = recvfrom (sock_server, &dp, sizeof (diskPage), 0, (sockaddr*)addr, &s);
   if (ret != sizeof (diskPage) && ret != -1)
    log (M_WARN, local_ip, "[THREAD_FUNC_NEIGHBOR] Strange diskpage received");

   if (ret == -1) { continue; }

   if (cache.is_valid (dp)) shiftedQuery++;
  }
 } while (_this->tserver_continue == true);

 pthread_exit (NULL); 
}
// }}}
// tfunc_client {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_client (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t sa = sizeof (_this->server_addr);
 //================================================
 do {
  if (!cache.queue_lower.empty ()) {

   diskPage DP = cache.get_low ();
   sendto (sock_left, &DP, sizeof (diskPage), 0, (sockaddr*)addr_left, s);
   SentShiftedQuery++;
  }

  if (!cache.queue_upper.empty ()) {

   diskPage DP = cache.get_upp ();
   sendto (sock_right, &DP, sizeof (diskPage), 0, (sockaddr*)addr_right, s); 
   SentShiftedQuery++;
  }
 } while (_this->tclient_continue == true);

 pthread_exit (NULL); 
}
// }}}
