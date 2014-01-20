#include <cache.hh>

// Cache::bind {{{
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
 log (M_INFO, "CACHEserverserver", "UDP server setted up [%s:%i]", 
   inet_ntoa (server_addr.sin_addr), ntohs (server_addr.sin_port));
#endif

 return true;
}
// }}}
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
bool Cache::lookup (int key, char* output, size_t* s) {
 try {
  Chunk* out = this->_map [key]; 

 } catch (std::out_of_range& e) {
  return false;
 }

 memcpy (output, out->str, out->size);
 *s = out->second;

 return false;
}
// }}}
// insert {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
bool Cache::insert (int key, char* output, size_t s) {
 char * tmp = new char (s);
 memcpy (tmp, output, s);
 this->_map [key] = PAIR (output, s);

 return true;
}
// }}}
// discard{{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void Cache::discard () {
 if        (policies & CACHE_LRU) {
  discard_lru ();

 } else if (policies & CACHE_SPATIAL) {
  discard_spatial ();
 }
}
// }}}
// Cache::tfunc_server {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_server (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t sa = sizeof (_this->server_addr);
 //================================================
  


 //================================================
 do {
  struct sockaddr_in client_addr;
  int ret = recvfrom (_this->server_fd, &input, 4, MSG_DONTWAIT, (struct sockaddr*)&client_addr, &sa); 

  if (ret == -1 && errno == EAGAIN) continue;                        //! No data received

  switch (ret) {
   case -1: log (M_DEBUG, "CACHEserver", "Error from recvfrom");      break; //! Error handle log
   case 0:  log (M_INFO, "CACHEserver", "Shutdown message received"); break; //! Handle shutdown
   default:                                                          //! Handle normal situation
            try {
             server_number = htonl (_this->table.at (ntohl(input)));

            } catch (std::out_of_range& e) {  // Not cached what to do ?
             server_number = htonl (CACHEserver_NOT_FOUND); 
#ifdef _DEBUG
             log (M_DEBUG, "CACHEserver", "Key not found");
#endif
            }

#ifdef _DEBUG
            log (M_DEBUG, "CACHEserver", "petition %u recieved -> %u", ntohl(input), ntohl (server_number));
#endif

            break; 
  }

  

 } while (_this->tserver_continue == true);

 pthread_exit (NULL); 
}
// }}}
// Cache::tfunc_client {{{
//                                -- Vicente Bolea
// ----------------------------------------------- 
void* Cache::tfunc_client (void* argv) {
 Cache* _this = (Cache*) argv;
 socklen_t sa = sizeof (_this->server_addr);
 //================================================
 do {

 } while (_this->tclient_continue == true);

 pthread_exit (NULL); 
}
// }}}
