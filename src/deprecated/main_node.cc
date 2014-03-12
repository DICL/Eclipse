/*
 * @file This file contains the source code of the application 
 *       which will run in each server 
 *
 *
 */
#include <node.hh>
#include <signal.h>
#include <simring.hh>

int main (int argc, const char** argv) {
 struct sockaddr_in addr_left, addr_right, addr_server;
 pthread_t thread_neighbor, thread_scheduler, thread_forward, thread_dht;
 struct Arguments args;
 
 signal (SIGINT,  catch_signal);
 signal (SIGSEGV, catch_signal);
 signal (SIGTERM, catch_signal);

 parse_args (argc, argv, &args);
 setup_client_scheduler (args.port, args.host_str, &sock_scheduler);
 setup_server_peer (args.port, &sock_server, &addr_server);

 if (args.peer_left)  setup_client_peer (args.port, args.peer_left, &sock_left, &addr_left);
 if (args.peer_right) setup_client_peer (args.port, args.peer_right, &sock_right, &addr_right);

 cache.setDataFile (args.data_file);
 struct sockaddr_in* addr_vec [2] = {&addr_left, &addr_right}; 

 pthread_create (&thread_scheduler, NULL, thread_func_scheduler, NULL);

#ifdef DATA_MIGRATION
   pthread_create (&thread_neighbor,  NULL, thread_func_neighbor,  &addr_server);
   pthread_create (&thread_forward,   NULL, thread_func_forward,   addr_vec);
   pthread_create (&thread_dht,       NULL, thread_func_dht,       NULL);
#endif

 pthread_join (thread_scheduler, NULL);

#ifdef DATA_MIGRATION
   pthread_join (thread_forward,   NULL);
   pthread_join (thread_neighbor,  NULL);
   pthread_join (thread_dht,       NULL);
#endif

 close_all ();

 return EXIT_SUCCESS;
}
