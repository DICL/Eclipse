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
 status = STATUS_VIRGIN;
 setted = 0;
}

Cache::~Cache () {
 this->close();
 pthread_barrier_destroy (&barrier_start);
 vector<int> sockets {Srequest, Smigration_server, Smigration_client};
 for_each (sockets.begin (), sockets.end(), ::close); 
 delete cache;
 delete DHT_client;
}
// }}}
// Setters {{{
// ----------------------------------------------- 
Cache& Cache::set_size    (size_t s)    {
 _size = s;
 setted |= SETTED_SIZE; 
 return *this; 
}
Cache& Cache::set_policy  (int p)       {
 policies = p;
 setted |= SETTED_POLICY; 
 return *this; 
}
Cache& Cache::set_port    (int port)    { 
 Pmigration = port; 
 Prequest = port + 1; 
 setted |= SETTED_PORT; 
 return *this; 
}
Cache& Cache::set_iface   (const char* ifa) {
 strncpy (local_ip_str, get_ip (ifa), INET_ADDRSTRLEN);
 local_ip = inet_addr (local_ip_str);
 setted |= SETTED_IFACE; 
 return *this; 
}
Cache& Cache::set_host    (const char* host_) {
 strncpy (host, host_, INET_ADDRSTRLEN);
 setted |= SETTED_HOST; 
 return *this; 
}
Cache& Cache::set_network (std::vector<const char*> in) {
 assert ((setted & SETTED_PORT) == SETTED_PORT);
 assert ((setted & SETTED_IFACE) == SETTED_IFACE);

 for (auto& ip : in) {
  struct sockaddr_in addr;
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons (this->Pmigration);
  addr.sin_addr.s_addr = inet_addr (ip);
  bzero (&(addr.sin_zero), 8);
  network.push_back (addr);
 }

 //! Get local index
 local_no = (find_if (network.begin(), network.end(), [=] (sockaddr_in &i){ 
    return (i.sin_addr.s_addr == local_ip); 
 })) - network.begin();

 setted |= SETTED_NETWORK; 
 return *this; 
}
// }}}
// bind {{{
// ----------------------------------------------- 
Cache& Cache::bind () {
 assert (setted == SETTED_ALL);
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
  log (M_ERR, local_ip_str, "socket function");

 if (setsockopt (Smigration_server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, local_ip_str, "setsockopt function");

 if (::bind (Smigration_server, (struct sockaddr*)&Amigration_server, sizeof (Amigration_server)) == -1)
  log (M_ERR, local_ip_str, "bind function");

 if ((Srequest = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) 
  log (M_ERR, local_ip_str, "socket function");

 if (setsockopt (Srequest, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, local_ip_str, "setsockopt function");

 if (::bind (Srequest, (struct sockaddr*)&Arequest, sizeof (Arequest)) == -1)
  log (M_ERR, local_ip_str, "bind function");

 DHT_client = new DHTclient (host, Pmigration + 3);
 cache = new SETcache (_size);

#ifdef _DEBUG
 log (M_INFO, local_ip_str, "MRR distributed cached setted\n"
   "\t- Migration socket setted  [%s:%i] UDP \n"
   "\t- Requesting socket setted [%s:%i] UDP \n", 
   inet_ntoa (Amigration_server.sin_addr), ntohs (Amigration_server.sin_port),
   inet_ntoa (Arequest.sin_addr), ntohs (Arequest.sin_port));
#endif

 status = STATUS_READY;
 return *this;
}
// }}}
// Thread functions {{{
// ----------------------------------------------- 
Cache& Cache::run () {
 assert (status == STATUS_READY);

 auto tfms = [](void *i) -> void* { ((Cache*)i)->migration_server (); pthread_exit (NULL);};
 auto tfmc = [](void *i) -> void* { ((Cache*)i)->migration_client (); pthread_exit (NULL);};
 auto tfr  = [](void *i) -> void* { ((Cache*)i)->request          (); pthread_exit (NULL);};

 pthread_create (&tmigration_server, NULL, (void* (*)(void*)) tfms, this);
 pthread_create (&tmigration_client, NULL, (void* (*)(void*)) tfmc, this);
 pthread_create (&trequest,          NULL, (void* (*)(void*)) tfr , this);
 status = STATUS_RUNNING;
 return *this;
}
// ----------------------------------------------- 
Cache& Cache::close () {
 tclient_continue = tserver_continue = tserver_request_continue = false;
 pthread_join (tmigration_server, NULL);
 pthread_join (tmigration_client, NULL);
 pthread_join (trequest, NULL);
 status = STATUS_CLOSED;
 return *this;
}
// }}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
std::tuple<char*, size_t> Cache::lookup (const char* key) throw (std::out_of_range) {
 try { 
  diskPage dp = cache->lookup (h (key, strlen (key)));
 } catch (std::out_of_range& e) {
  request (key);  
 }
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

 //// Code to ask DHT :TODO:
 //if (ret == false) {
 // int victim = DHT_client->lookup (output);
 // if (victim != local_no) {
 //  request (key);
 // }
 //}

 return ret;
}
//}}}
// Request {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Cache::request (const char * key) {
 assert (key != NULL);

 socklen_t s = sizeof (struct sockaddr);
 uint16_t server_no = DHT_client->lookup (key);

 if (server_no == local_no) return false; 
 uint16_t message = htons (server_no);

 sendto (Srequest, &message, 2, 0,(sockaddr*)&network [server_no], s);

 return true;
}
// }}}
// Migration_server {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::migration_server () {
 socklen_t sa = sizeof (Amigration_server);
 pthread_barrier_wait (&barrier_start);
 //================================================
 do {
  diskPage dp;
  if (fd_is_ready (Smigration_server)) {

   int ret = recvfrom (Smigration_server, &dp, sizeof (diskPage), 0, (sockaddr*)&Amigration_server, &sa);
   if (ret != sizeof (diskPage) && ret != -1)
    log (M_WARN, local_ip_str, "[THREAD_FUNC_NEIGHBOR] Strange diskpage received");

   if (ret == -1) { continue; }
   if (cache->is_valid (dp)) stats["shiftedQuery"]++;
  }
 } while (tserver_continue == true);
}
// }}}
// Migration_client {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::migration_client () {
 socklen_t sa = sizeof (Amigration_server);
 size_t _size = network.size();
 int sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);

 int left_idx  = (local_no == 0) ? _size: local_no - 1;
 int right_idx = ((size_t)local_no == _size) ? 0: local_no + 1;

 struct sockaddr_in* addr_left  = &(network [left_idx]);
 struct sockaddr_in* addr_right = &(network [right_idx]);
 pthread_barrier_wait (&barrier_start);
 //================================================

 do {
  if (!cache->queue_lower.empty ()) {

   diskPage DP = cache->get_low ();
   sendto (sock, &DP, sizeof (diskPage), 0, (sockaddr*)addr_left, sa);
   stats ["SentShiftedQuery"]++;
  }

  if (!cache->queue_upper.empty ()) {

   diskPage DP = cache->get_upp ();
   sendto (sock, &DP, sizeof (diskPage), 0, (sockaddr*)addr_right, sa); 
   stats ["SentShiftedQuery"]++;
  }
 } while (tclient_continue == true);
}
// }}}
// Request {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::request () {
 socklen_t s = sizeof (Arequest);
 pthread_barrier_wait (&barrier_start);
 //================================================

 do {
  Header Hrequested;
  struct sockaddr_in client_addr;
  if (fd_is_ready (Srequest)) {
   ssize_t ret = recvfrom (Srequest, &Hrequested, sizeof (Header), 0, (sockaddr*)&client_addr, &s);
   if (ret == sizeof (Header)) {

    stats["ReceivedData"]++;
    diskPage DPrequested = cache->get_diskPage (Hrequested.get_point ()); //! Get the diskPage
    client_addr.sin_port = htons (Pmigration);                            //! change to Mig port

    if (Hrequested.trace)
     log (M_DEBUG, local_ip_str, "Received a petition of requested data from %s:%i", 
       inet_ntoa (client_addr.sin_addr), Prequest);

    sendto (Srequest, &DPrequested, sizeof (diskPage), 0, (sockaddr*)&client_addr, s); //:TRICKY:
   }
  }
 } while (tserver_continue == true);
}
// }}}
