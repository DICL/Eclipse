//
// @file This file contains the source code of the application 
//       which will run in each server 
//
// ---------------------------------------------------
// includes {{{
// ---------------------------------------------------
//
#include <node_sketch.hh>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <cfloat>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <queue>
//
//
// }}}
// constructor {{{
// ---------------------------------------------------
//
Node::Node (int argc, const char ** argv, const char * ifa, const char ** net) {
 queryRecieves = 0;
 queryProcessed = 0;
 hitCount = 0;
 missCount = 0;
 TotalExecTime = 0;
 TotalWaitTime = 0;

 sch_port  = 20000;
 peer_port = 20001;
 dht_port  = 20002;

 signal (SIGTERM, Node::signal_handler);
 signal (SIGSEGV, Node::signal_handler);
 signal (SIGKILL, Node::signal_handler);

 parse_args (argc, argv);
 local_ip = get_ip (ifa);
 dht.set_network (dht_port, 10, local_ip, network_ip);

 setup_client_scheduler (sch_port, host_str, &sock_scheduler);
 setup_server_peer (peer_port, &sock_server, &addr_server);

 if (local_no > 0)        strcpy (peer_left, network_ip [local_no - 1]);
 if (local_no < nservers) strcpy (peer_right, network_ip [local_no + 1]);
 if (peer_left)  setup_client_peer (peer_port, peer_left, &sock_left, &addr_left);
 if (peer_right) setup_client_peer (peer_port, peer_right, &sock_right, &addr_right);

 cache.setDataFile (data_file);
 instance = *this;
}
//
// }}}
// destructor {{{
// ---------------------------------------------------
//
Node::~Node () { close_all (); }
//
// }}}
// run {{{
// ---------------------------------------------------
//
void Node::run () {
 pthread_create (&thread_scheduler, NULL, thread_func_scheduler, NULL);

#ifdef DATA_MIGRATION
 pthread_create (&thread_neighbor,  NULL, thread_func_neighbor,  &addr_server);
 pthread_create (&thread_forward,   NULL, thread_func_forward,   addr_vec);
 pthread_create (&thread_dht,       NULL, thread_func_dht,       NULL);
#endif
}
//
// }}}
// join {{{
// ---------------------------------------------------
//
void Node::join () {
 pthread_join (thread_scheduler, NULL);

#ifdef DATA_MIGRATION
 pthread_join (thread_forward,   NULL);
 pthread_join (thread_neighbor,  NULL);
 pthread_join (thread_dht,       NULL);
#endif
}
//
// }}}
// thread_func_dht {{{
// ---------------------------------------------------
//
void* Node::thread_func_dht (void* arg) {
 int sock_server_dht;
 struct sockaddr_in addr;
 socklen_t s = sizeof (addr);

 //! Setup the addr of the server
 sock_server_dht = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);

 addr.sin_family = AF_INET;
 addr.sin_port = htons (DHT_PORT);
 addr.sin_addr.s_addr = htonl (INADDR_ANY);
 bzero (&(addr.sin_zero), 8);

 bind (sock_server_dht, (sockaddr*)&addr, s);

 while (!panic) {
  if (fd_is_ready (sock_server_dht)) {
   Header Hrequested;
   struct sockaddr_in client_addr;

   //! Read a new query
   ssize_t ret = recvfrom (sock_server_dht, &Hrequested, sizeof (Header), 0, (sockaddr*)&client_addr, &s);

   if (ret == sizeof (Header)) {

    ReceivedData++;
    //! load the data first from memory after from HD
    diskPage DPrequested = cache.get_diskPage (Hrequested.get_point ());

    //! Send to the ip which is asking for it
    client_addr.sin_port = htons (PEER_PORT);

    char address [INET_ADDRSTRLEN];
    inet_ntop (AF_INET, &client_addr.sin_addr, address, INET_ADDRSTRLEN);
    if (Hrequested.trace)
     log (M_DEBUG, local_ip, "Received a petition of requested data from %s", address);

    sendto (sock_server_dht, &DPrequested, sizeof (diskPage), 0, (sockaddr*)&client_addr, s); //:TRICKY:
   }
  }
 }

 close (sock_server_dht);
 pthread_exit (EXIT_SUCCESS);
}
//
// }}}
// thread_func_scheduler {{{
// ---------------------------------------------------
// @brief  Thread function to receive queries from the scheduler.
//         This function can be seen as one of the producers.
// @args   Dummy parameter
// ---------------------------------------------------
void * Node::thread_func_scheduler (void * argv) {
 for (char recv_data [LOT]; !panic; bzero (&recv_data, LOT)) {
  recv_msg (sock_scheduler, recv_data);

  //! When a new query arrive
  if (strcmp (recv_data, "QUERY") == OK) {
   Query query;

   query.setScheduledDate ();

   int bytes_sent = recv (sock_scheduler, &query, sizeof(Packet), 0);
   if (bytes_sent != sizeof (Packet)) {
    log (M_WARN, local_ip, "I Received a strange length data");
    continue;
   }

   if (query.trace) 
    log (M_DEBUG, local_ip, "[QUERY: %i] arrived from scheduler",query.get_point());

   query.setStartDate ();                                           
   bool found = cache.match (query); //! change it
   query.setFinishedDate ();                                        

   if (query.trace) {
    if (found) 
     log (M_DEBUG, local_ip, "[QUERY: %i] found in the cache",query.get_point());
    else 
     log (M_DEBUG, local_ip, "[QUERY: %i] not found in the cache",query.get_point());
   }

   if (!found && !dht.check (query)) 
     if (dht.request (query)) RequestedData++;

   if (found) hitCount++; else missCount++;

   queryProcessed++;                                                
   queryRecieves++;

   TotalExecTime += query.getExecTime ();                            
   TotalWaitTime += query.getWaitTime ();                           

   //! When it ask for information
  } else if (strcmp (recv_data, "INFO") == OK) {
   char send_data [LOT] = "", tmp [256];

   sprintf (tmp, "CacheHit=%"         PRIu64 "\n", hitCount);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "CacheMiss=%"        PRIu64 "\n", missCount);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "QueryCount=%"       PRIu32 "\n", queryProcessed);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "TotalExecTime=%"    PRIu64 "\n", TotalExecTime);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "TotalWaitTime=%"    PRIu64 "\n", TotalWaitTime);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "TotalWaitTime=%"    PRIu64 "\n", TotalWaitTime);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "shiftedQuery=%"     PRIu64 "\n", shiftedQuery);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "SentShiftedQuery=%" PRIu64 "\n", SentShiftedQuery);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "RequestedData=%"    PRIu64 "\n", RequestedData);
   strncat (send_data, tmp, 256);
   sprintf (tmp, "ReceivedData=%"     PRIu64 "\n", ReceivedData);
   strncat (send_data, tmp, 256);

   _send (sock_scheduler, send_data, LOT, 0);

   //! In case that we need to finish the execution 
  } else if (strcmp (recv_data, "QUIT") == OK) {
   panic = true;
   sleep (1);

  } else {
   log (M_WARN, local_ip, "Unknown message received [MSG: %s]", recv_data);
   panic = true;
  }
 }
 pthread_exit (EXIT_SUCCESS);
}
//
// }}}
// thread_func_neighbor {{{
// ---------------------------------------------------
// @brief  Thread function to receive queries from the scheduler.
//         This function can be seen as one of the producers.
// @args   Dummy parameter
// ---------------------------------------------------
//
void * Node::thread_func_neighbor (void* argv) {
 socklen_t s = sizeof (sockaddr);
 assert (addr->sin_family == AF_INET);

 while (!panic) {
  diskPage dp;
  
  if (fd_is_ready (sock_server)) {

   int ret = recvfrom (sock_server, &dp, sizeof (diskPage), 0, (sockaddr*)addr_server, &s);
   if (ret != sizeof (diskPage) && ret != -1) 
     log (M_WARN, local_ip, "[THREAD_FUNC_NEIGHBOR] Strange diskpage received");

   if (ret == -1) { continue; }

   if (cache.is_valid (dp)) shiftedQuery++;
  }
 }
 pthread_exit (EXIT_SUCCESS);
}
//
// }}}
// thread_func_forward {{{
// ---------------------------------------------------
//
void * Node::thread_func_forward (void * argv) {
 socklen_t s = sizeof (struct sockaddr);	

 while (!panic) {
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
 }
 pthread_exit (EXIT_SUCCESS);
}
//
//
//---------------------------------------------------------------------//
//-----------SETTING UP FUNCTIONS--------------------------------------//
//---------------------------------------------------------------------//
//
// }}}
// setup_server_peer {{{
// ---------------------------------------------------
//
void Node::setup_server_peer (int port, int* sock, sockaddr_in* addr) {
 socklen_t s = sizeof (sockaddr);
 EXIT_IF (*sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP), "SOCKET");

 addr->sin_family      = AF_INET;
 addr->sin_port        = htons (PEER_PORT);
 addr->sin_addr.s_addr = htonl (INADDR_ANY);
 bzero (&(addr->sin_zero), 8);

 EXIT_IF (bind (*sock, (sockaddr*)addr, s), "BIND PEER");
}
//
// }}}
// setup_client_peer {{{
// ---------------------------------------------------
//
void 
Node::setup_client_peer (const int port, const char* host, int* sock, sockaddr_in* addr)
{
 EXIT_IF (*sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP), "SOCKET");

 addr->sin_family      = AF_INET;
 addr->sin_port        = htons (PEER_PORT);
 addr->sin_addr.s_addr = inet_addr (host);
 bzero (&(addr->sin_zero), 8);
}
//
// }}}
// setup_client_scheduler {{{
// ---------------------------------------------------
//
void Node::setup_client_scheduler (int port, const char* host, int* sock) {
 struct sockaddr_in server_addr;  
 socklen_t s = sizeof (sockaddr);

 EXIT_IF (*sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP), "SOCKET SCHEDULER");

 server_addr.sin_family      = AF_INET;
 server_addr.sin_port        = htons (port);
 server_addr.sin_addr.s_addr = inet_addr (host);
 bzero (&(server_addr.sin_zero), 8);

 EXIT_IF (connect (*sock, (sockaddr*)&server_addr, s), "CONNECT SCHEDULER");
}
//
// }}}
// parse_args {{{
// @brief parse the command line options
// @param number or args
// @param array of args 
// ---------------------------------------------------
//
void Node::parse_args (int argc, const char** argv) {
 int c = 0;  
 do {
  switch (c) {
   case 'h': strncpy (host_str, optarg, 32);   break;
   case 'd': strncpy (data_file, optarg, 256); break;
   case 'p': port = atoi (optarg);             break;
  }
  c = getopt (argc, const_cast<char**> (argv), "h:d:p:r:l:");
 } while (c != -1);

 // Check if everything was set
 if (!host_str || !data_file || !port)
  log (M_ERR, local_ip, "PARSER: Arguments needs to be setted");
}
//
// }}}
// signal_handler {{{
// ---------------------------------------------------
//
void Node::signal_handler () { Node.catch_signal(); }
//
// }}}
// catch_signal {{{
// ---------------------------------------------------
//
void Node::catch_signal (int arg) {
 close_all ();
 log (M_ERR, local_ip, "KILL Signal received, sockets closed");
}
//
// }}}
// close_all {{{
// ---------------------------------------------------
//
void Node::close_all () {
 close (sock_scheduler);
 close (sock_left);
 close (sock_right);
 close (sock_server);
}
