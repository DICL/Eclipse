#include <cache.hh>
#include <assert.h>
#include <string.h>
#include <algorithm>
#include <functional>

// Constructors {{{
// ----------------------------------------------- 
Cache::Cache () : _size (CACHE_DEFAULT_SIZE) { 
 tserver_continue = true; 
 tclient_continue = true; 
 tserver_request_continue = true;
 pthread_barrier_init (&barrier_start, NULL, number_threads);
}

Cache::Cache (size_t s, uint8_t p) : Cache () { 
 this->cache = new SETcache (s);
 this->_size = s; 
 this->policies = p; 
}

Cache::~Cache () {
 pthread_barrier_destroy (&barrier_start);
 vector<int> sockets {Srequest, Smigration_server, Smigration_client};
 for_each (sockets.begin (), sockets.end(), ::close); 
 delete cache;
 delete DHT_client;
}
// }}}
// bind {{{
// ----------------------------------------------- 
bool Cache::bind () {
 int one = 1;

 bzero (&(Amigration_server.sin_zero), 8);
 Amigration_server.sin_family = AF_INET;
 Amigration_server.sin_port = htons (Pmigration);
 Amigration_server.sin_addr.s_addr = htonl (INADDR_ANY);

 bzero (&(Arequest.sin_zero), 8);
 Arequest.sin_family = AF_INET;
 Arequest.sin_port = htons (Prequest);
 Arequest.sin_addr.s_addr = htonl (INADDR_ANY);

 if ((Smigration_server = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  log (M_ERR, "CACHEserver", "socket function");

 if (setsockopt (Smigration_server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "CACHEserver", "setsockopt function");

 if (::bind (Smigration_server, (struct sockaddr*)&Amigration_server, sizeof (Amigration_server)) == -1)
  log (M_ERR, "CACHEserver", "bind function");

 if ((Srequest = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) 
  log (M_ERR, "CACHEserver", "socket function");

 if (setsockopt (Srequest, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, "CACHEserver", "setsockopt function");

 if (::bind (Srequest, (struct sockaddr*)&Arequest, sizeof (Arequest)) == -1)
  log (M_ERR, "CACHEserver", "bind function");

#ifdef _DEBUG
 log (M_INFO, "CACHEserver", "MRR distributed cached setted           \n"
                             "\t- Migration socket setted  [%s:%i] UDP \n"
                             "\t- Requesting socket setted [%s:%i] UDP \n", 
   inet_ntoa (Amigration_server.sin_addr), ntohs (Amigration_server.sin_port),
   inet_ntoa (Arequest.sin_addr), ntohs (Arequest.sin_port));
#endif

 return true;
}
// }}}
// set_network {{{
// ----------------------------------------------- 
void Cache::set_network (int port, int n, const char* ifa, const char ** in, const char * host) {
 assert (in != NULL);

 if (ifa == NULL)
  ifa = const_cast<const char*> (strdup ("eth0"));

 Pmigration  = port;
 Prequest    = port + 1; 

 network.resize ((size_t) n);

 strncpy (local_ip_str, get_ip (ifa), INET_ADDRSTRLEN);
 local_ip = inet_addr (local_ip_str);
 
 int j = 0;
 for (auto& addr : network) {
  //int index = std::find (network.begin(), network.end(), addr) - network.begin();
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons (this->Pmigration);
  addr.sin_addr.s_addr = inet_addr (in [j++]);
  bzero (&(addr.sin_zero), 8);
 }

 //! Get local index
 local_no = (find_if (network.begin(), network.end(), [=] (sockaddr_in &i){ 
   return (i.sin_addr.s_addr == local_ip); 
 })) - network.begin();

 DHT_client = new DHTclient (host, port + 3);
}
//}}}
// run {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::run () {
 pthread_create (&tmigration_server, NULL, tfunc_migration_server, this);
 pthread_create (&tmigration_client, NULL, tfunc_migration_client, this);
 pthread_create (&trequest,          NULL, tfunc_request, this);
}
// }}}
// close {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::close () {
 pthread_join (tmigration_server, NULL);
 pthread_join (tmigration_client, NULL);
 pthread_join (trequest, NULL);
}
// }}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
std::tuple<char*, size_t> Cache::lookup (const char* key) throw (std::out_of_range) {
 diskPage dp = cache->lookup (h (key, strlen (key)));
 return std::make_tuple (dp.chunk, DPSIZE);
}
// }}}
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Cache::insert (const char* key, const char* output, size_t s) {
 if (s == 0) s = strlen (output);
 diskPage dp (h (key, strlen (key)));
 memcpy (dp.chunk, output, DPSIZE);
 bool ret = cache->insert (dp);

 // Code to ask DHT :TODO:
 if (ret == false) {
  int victim = DHT_client->lookup (output);
  if (victim != local_no) {
   request (key);
  }
 }

 return ret;
}
//}}}
// Request {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Cache::request (const char * key) {
 assert (key != NULL);

 socklen_t s = sizeof (struct sockaddr);
 uint8_t server_no = DHT_client->lookup (key);

 if (server_no == local_no) return false; 
 uint8_t message = htons (server_no);

 sendto (Srequest, &message, 1, 0,(sockaddr*)&network [server_no], s);

 return true;
}
// }}}
// tfunc_server {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_migration_server (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t sa = sizeof (_this->Amigration_server);
 pthread_barrier_wait (&_this->barrier_start);
 //================================================
 do {
  diskPage dp;
  if (fd_is_ready (_this->Smigration_server)) {

   int ret = recvfrom (_this->Smigration_server, &dp, sizeof (diskPage), 0, (sockaddr*)&_this->Amigration_server, &sa);
   if (ret != sizeof (diskPage) && ret != -1)
    log (M_WARN, _this->local_ip_str, "[THREAD_FUNC_NEIGHBOR] Strange diskpage received");

   if (ret == -1) { continue; }

   //if (_this->cache->is_valid (dp)) // :TODO:  shiftedQuery++;
  }
 } while (_this->tserver_continue == true);

 pthread_exit (NULL); 
}
// }}}
// tfunc_client {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_migration_client (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t sa = sizeof (_this->Amigration_server);
 size_t _size = _this->network.size();
 int sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);

 int left_idx  = (_this->local_no == 0) ? _size: _this->local_no - 1;
 int right_idx = ((size_t)_this->local_no == _size) ? 0: _this->local_no + 1;

 struct sockaddr_in* addr_left  = &(_this->network [left_idx]);
 struct sockaddr_in* addr_right = &(_this->network [right_idx]);
 pthread_barrier_wait (&_this->barrier_start);
 //================================================

 do {
  if (!_this->cache->queue_lower.empty ()) {

   diskPage DP = _this->cache->get_low ();
   sendto (sock, &DP, sizeof (diskPage), 0, (sockaddr*)addr_left, sa);
   (_this->stats ["SentShiftedQuery"])++;
  }

  if (!_this->cache->queue_upper.empty ()) {

   diskPage DP = _this->cache->get_upp ();
   sendto (sock, &DP, sizeof (diskPage), 0, (sockaddr*)addr_right, sa); 
   (_this->stats ["SentShiftedQuery"])++;
  }
 } while (_this->tclient_continue == true);

 pthread_exit (NULL); 
}
// }}}
// tfunc_server_request {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_request (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t s = sizeof (_this->Arequest);
 pthread_barrier_wait (&_this->barrier_start);
 //================================================

 do {
  Header Hrequested;
  struct sockaddr_in client_addr;
  if (fd_is_ready (_this->Srequest)) {
    //! Read a new query
    ssize_t ret = recvfrom (_this->Srequest, &Hrequested, sizeof (Header), 0, (sockaddr*)&client_addr, &s);

    if (ret == sizeof (Header)) {

    //ReceivedData++;
    //! load the data first from memory after from HD
    diskPage DPrequested = _this->cache->get_diskPage (Hrequested.get_point ());

    //! Send to the ip which is asking for it
    client_addr.sin_port = htons (_this->Prequest);

    char address [INET_ADDRSTRLEN];
    inet_ntop (AF_INET, &client_addr.sin_addr, address, INET_ADDRSTRLEN);

    if (Hrequested.trace)
     log (M_DEBUG, _this->local_ip_str, "Received a petition of requested data from %s:%i", address, _this->Prequest);

    sendto (_this->Srequest, &DPrequested, sizeof (diskPage), 0, (sockaddr*)&client_addr, s); //:TRICKY:
    }
  }
 } while (_this->tserver_continue == true);

 pthread_exit (NULL); 
}
// }}}
