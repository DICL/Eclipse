#include <cache.hh>
#include <assert.h>
#include <string.h>
#include <algorithm>
#include <functional>
//#include <tuple>

// Constructors {{{
// ----------------------------------------------- 
Cache::Cache () : _size (CACHE_DEFAULT_SIZE) { 
 tserver_continue = true; 
 tclient_continue = true; 
 tserver_request_continue = true;
 pthread_barrier_init (&barrier_start, NULL, 3);
}

Cache::Cache (size_t s, uint8_t p = 0) : Cache () { 
// ..Cache();
 this->cache = new SETcache (s);
 this->_size = s; 
 this->policies = p; 
}

Cache::~Cache () {
 pthread_barrier_destroy (&barrier_start);
 ::close (sock_server); 
 ::close (sock_scheduler); 
 ::close (sock_left);
 ::close (sock_right);

 delete cache;
}
// }}}
// bind {{{
// ----------------------------------------------- 
bool Cache::bind () {
 int one = 1;

 bzero (&(server_addr.sin_zero), 8);
 server_addr.sin_family = AF_INET;
 server_addr.sin_port = htons (port_server);
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

 if (ifa == NULL)
  ifa = const_cast<const char*> (strdup ("eth0"));

 this->port_server = port;
 network.resize ((size_t) n);

 local_ip = inet_addr (get_ip (ifa));

 int i = 0; 
 for (auto& addr : network) {
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons (this->port_dht);
  addr.sin_addr.s_addr = inet_addr (in[i]);
  bzero (&(addr.sin_zero), 8);
  i++;
 }

 //! Get local index
 local_no = (find_if (network.begin(), network.end(), [=] (sockaddr_in &i){ 
   return (i.sin_addr.s_addr == local_ip); 
 })) - network.begin();

 //for (int i = 0; local_ip != network[i].sin_addr.s_addr; i++);
 sock_request = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
 struct in_addr local_ip_addr;
 local_ip_addr.s_addr = local_ip;

 using namespace std::placeholders;
 auto log_aux = std::bind (
   static_cast<void(*)(int,const char*,const char*,...)> (&log), _1,
   const_cast<const char*> (inet_ntoa (local_ip_addr)), _2);

 log_error = std::bind (log_aux, M_ERR, _1);
 log_debug = std::bind (log_aux, M_DEBUG, _1);
 log_warn = std::bind (log_aux, M_WARN, _1);
}
//}}}
// run {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::run () {
 pthread_create (&tserver, NULL, tfunc_server, this);
 pthread_create (&tserver, NULL, tfunc_server_request, this);
 pthread_create (&tserver, NULL, tfunc_client, this);
}
// }}}
// close {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::close () {
 pthread_join (tclient, NULL);
 pthread_join (tserver_request, NULL);
 pthread_join (tserver, NULL);
}
// }}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
std::tuple<char*, size_t> Cache::lookup (int key) throw (std::out_of_range) {
 diskPage dp = cache->lookup (key);
 return std::make_tuple (dp.chunk, DPSIZE);
}
// }}}
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Cache::insert (int key, char* output, size_t s) {
 diskPage dp (key);
 memcpy (dp.chunk, output, DPSIZE);
 bool ret = cache->insert (dp);

 // Code to ask DHT :TODO:
 //if (ret == false) {
 // int victim = DHTclient_i.lookup (output);
 // if (victim != local_no) {
 //  request (key, output, victim);
 // }
 //}

 return ret;
}
//}}}
// Request {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Cache::request (Header& h) {
 assert (&h != NULL);

 socklen_t s = sizeof (struct sockaddr);
 int server_no = DHTclient_i.lookup (h.get_point());

 if (server_no == local_no) return false; 
 sendto (sock_request, &h, sizeof (Header), 0,(sockaddr*)&network [server_no], s);

 //if (h.trace)
 // log (M_DEBUG, local_ip, "Request sent waiting for response from %s",
 //   network_ip [server_no]);

 return true;
}
// }}}
// tfunc_server {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_server (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t sa = sizeof (_this->server_addr);
 pthread_barrier_wait (&_this->barrier_start);
 //================================================
 do {
  diskPage dp;
  if (fd_is_ready (_this->sock_server)) {

   int ret = recvfrom (_this->sock_server, &dp, sizeof (diskPage), 0, (sockaddr*)&_this->server_addr, &sa);
   if (ret != sizeof (diskPage) && ret != -1)
    _this->log_warn ("[THREAD_FUNC_NEIGHBOR] Strange diskpage received");

   if (ret == -1) { continue; }

   //if (_this->cache->is_valid (dp)) // :TODO:  shiftedQuery++;
  }
 } while (_this->tserver_continue == true);

 pthread_exit (NULL); 
}
// }}}
// tfunc_server_request {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_server_request (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t sa = sizeof (_this->server_addr);
 int sock_server_dht;
 struct sockaddr_in addr;
 socklen_t s = sizeof (addr);

 pthread_barrier_wait (&_this->barrier_start);
 //================================================

 //! Setup the addr of the server
 sock_server_dht = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);

 addr.sin_family = AF_INET;
 addr.sin_port = htons (DHT_PORT);
 addr.sin_addr.s_addr = htonl (INADDR_ANY);
 bzero (&(addr.sin_zero), 8);

 ::bind (sock_server_dht, (sockaddr*)&addr, s);

 //================================================

 do {
  Header Hrequested;
  struct sockaddr_in client_addr;
  if (fd_is_ready (sock_server_dht)) {
    //! Read a new query
    ssize_t ret = recvfrom (sock_server_dht, &Hrequested, sizeof (Header), 0, (sockaddr*)&client_addr, &s);

    if (ret == sizeof (Header)) {

    //ReceivedData++;
    //! load the data first from memory after from HD
    diskPage DPrequested = _this->cache->get_diskPage (Hrequested.get_point ());

    //! Send to the ip which is asking for it
    client_addr.sin_port = htons (_this->port_dht);

    char address [INET_ADDRSTRLEN];
    inet_ntop (AF_INET, &client_addr.sin_addr, address, INET_ADDRSTRLEN);
    if (Hrequested.trace)
    _this->log_debug ("Received a petition of requested data from %s:%i", address, _this->port);

    sendto (sock_server_dht, &DPrequested, sizeof (diskPage), 0, (sockaddr*)&client_addr, s); //:TRICKY:

    }
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
 size_t _size = _this->network.size();

 left_idx  = (local_no == 0) ? _size: local_no - 1;
 right_idx = (local_no == _size) ? 0: local_no + 1;

 struct sockaddr_in addr_left*  = &network [left_idx];
 struct sockaddr_in addr_right* = &network [right_idx];
 pthread_barrier_wait (&_this->barrier_stop);
 //================================================

 do {
  if (!cache.queue_lower.empty ()) {

   diskPage& DP = cache.get_low ();
   sendto (sock_left, &DP, sizeof (diskPage), 0, (sockaddr*)addr_left, s);
   SentShiftedQuery++;
  }

  if (!cache.queue_upper.empty ()) {

   diskPage& DP = cache.get_upp ();
   sendto (sock_right, &DP, sizeof (diskPage), 0, (sockaddr*)addr_right, s); 
   SentShiftedQuery++;
  }
 } while (_this->tclient_continue == true);

 pthread_exit (NULL); 
}
// }}}
