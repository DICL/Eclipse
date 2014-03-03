#include <orthrus.hh>
#include <assert.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <sys/utsname.h>

// Constructors {{{
// ----------------------------------------------- 
Orthrus::Orthrus () : _size (CACHE_DEFAULT_SIZE) { 
 tserver_continue = true; 
 tclient_continue = true; 
 tserver_request_continue = true;
 pthread_barrier_init (&barrier_start, NULL, number_threads);
 status = STATUS_VIRGIN;
 setted = 0;
}

Orthrus::~Orthrus () {
 if (status != STATUS_CLOSED) this->close();
 pthread_barrier_destroy (&barrier_start);
 delete cache;
 delete DHT_client;
}
// }}}
// Setters {{{
// ----------------------------------------------- 
Orthrus& Orthrus::set_size    (size_t s)    {
 _size = s;
 setted |= SETTED_SIZE; 
 return *this; 
}
Orthrus& Orthrus::set_policy  (int p)       {
 policies = p;
 setted |= SETTED_POLICY; 
 return *this; 
}
Orthrus& Orthrus::set_port    (int port)    { 
 Pmigration = port; 
 Prequest = port + 1; 
 setted |= SETTED_PORT; 
 return *this; 
}
Orthrus& Orthrus::set_iface   (const char* ifa) {
 strncpy (local_ip_str, get_ip (ifa), INET_ADDRSTRLEN);
 local_ip = inet_addr (local_ip_str);
 setted |= SETTED_IFACE; 
 return *this; 
}
Orthrus& Orthrus::set_host    (const char* host_) {
 strncpy (host, host_, INET_ADDRSTRLEN);
 setted |= SETTED_HOST; 
 return *this; 
}
Orthrus& Orthrus::set_network (std::vector<const char*> in) {
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
Orthrus& Orthrus::bind () {
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
 cache = new Orthrus (_size);

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
Orthrus& Orthrus::run () {
 assert (status == STATUS_READY);

 pthread_create (&tmigration_server, NULL, [](void *i) -> void* { 
  ((Orthrus*)i)->migration_server (); pthread_exit (NULL);
 }, this);
 pthread_create (&tmigration_client, NULL, [](void *i) -> void* {
  ((Orthrus*)i)->migration_client (); pthread_exit (NULL);
 }, this);
 pthread_create (&trequest,          NULL, [](void *i) -> void* { 
  ((Orthrus*)i)->request_listener (); pthread_exit (NULL);
 }, this);
 status = STATUS_RUNNING;
 return *this;
}
// ----------------------------------------------- 
Orthrus& Orthrus::close () {
 tclient_continue = tserver_continue = tserver_request_continue = false;
 pthread_join (tmigration_server, NULL);
 pthread_join (tmigration_client, NULL);
 pthread_join (trequest, NULL);
 vector<int> sockets {Srequest, Smigration_server, Smigration_client};
 for_each (sockets.begin (), sockets.end(), ::close); 
 status = STATUS_CLOSED;
 return *this;
}
// }}}
// lookup {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
std::tuple<char*, size_t> Orthrus::lookup (std::string key) throw (std::out_of_range) {
 diskPage dp;
 std::hash<std::string> pt_hash; 
 try { 
  dp = cache->lookup (pt_hash (key));
 } catch (std::out_of_range& e) {
  request (key.c_str());  
 }
 return std::make_tuple ((dp.chunk), 10000);
}
// }}}
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Orthrus::insert (std::string key, std::string val) {
 std::hash<std::string> pt_hash; 
 size_t index = pt_hash (key);
 diskPage dp (index);
 memcpy (dp.chunk, val.c_str(), DPSIZE);
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
bool Orthrus::request (const char * key) {
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
void Orthrus::migration_server () {
 socklen_t sa = sizeof (Amigration_server);
 pthread_barrier_wait (&barrier_start);
 //================================================
 do {
  diskPage dp;
  if (fd_is_ready (Smigration_server)) {

   int ret = recvfrom (Smigration_server, &dp, sizeof (diskPage), 0, (sockaddr*)&Amigration_server, &sa);
   if (ret != sizeof (diskPage) and ret != -1)
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
void Orthrus::migration_client () {
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
// Request_listener {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Orthrus::request_listener () {
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
// print cache {{{
//
// It assume that the string is at maximum of 32 bits,
// also assume that the binary has maximum 32 bits width 
void bin_to_str (int binary, char* str, size_t size) {
 if (size != 32 and size != 16 and size != 8) return;
 size_t i;
 for (i = 2; i <= size - 2; i++)
  str [size-i] = (binary & (1<<(i-2))) ? '1': '0';
 str [0] = '0';
 str [1] = 'b';
 str [size - 1] = '\0';
}
void Orthrus::print_cache () {
 const char * msg = 
 "--------------------------------------------------\n"
 " +++++++ Orthrus debug info                       \n"
 "--------------------------------------------------\n"
 " + Status:         | %16s                         \n"
 " + Policy:         | %16s                         \n"
 " + Opts. setted:   | %16s                         \n"
 " + Size:           | %16d disk pages              \n"
 " + Memory usage:   | %16d KiB                     \n"
 " + Local IP:       | %16s                         \n"
 " + Local index:    | %16d                         \n"
 " + Host IP:        | %16s                         \n"
 " + Architecture:   | %16s                         \n"
 " + Version:        | %16s                         \n"
 "--------------------------------------------------\n";

 char bset[16], bpol [16], bstat [16];
 struct utsname info;
 uname (&info);
 bin_to_str (status, bstat, 16);
 bin_to_str (policies, bpol, 16);
 bin_to_str (setted, bset, 16);
 
 printf (msg, bstat, bpol, bset, _size, _size * 8, 
         local_ip_str, local_no, host, info.machine, info.release);
}
// }}}
