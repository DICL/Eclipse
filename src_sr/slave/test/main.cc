#define _DEBUG
#include "node.hh"
#include <signal.h>

int main (int argc, const char** argv) {
 _connect = connect_mock;               //! Mock function
 _recv = recv_mock;                     //! My mocking functions
 _send = send_mock;                     //! my mocking functions 

 signal (SIGINT, catch_signal);
 signal (SIGSEGV, catch_signal);
 signal (SIGKILL, catch_signal);
 signal (SIGQUIT, catch_signal);

 parse_args (argc, argv);
 setup_client_scheduler (host_str);
 setup_server_peer (port);
 setup_client_peer (port, peer_right, peer_left);

// pthread_mutex_lock (&mutex_scheduler); //! Initialize the lock to 0
// pthread_mutex_lock (&mutex_neighbor);  //! Initialize the lock to 0

 pthread_create (&thread_disk,      NULL, thread_func_disk,      NULL);
 pthread_create (&thread_neighbor,  NULL, thread_func_neighbor,  NULL);
 pthread_create (&thread_scheduler, NULL, thread_func_scheduler, NULL);

 pthread_join (thread_scheduler, NULL);
 pthread_join (thread_disk,      NULL);
 pthread_join (thread_neighbor,  NULL);

 close_all ();

 return EXIT_SUCCESS;
}
