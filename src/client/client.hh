#ifndef __CLIENT__
#define __CLIENT__

int connect_to_server(char *host, unsigned short port); // function which connect to the master
void *signal_listener(void *args); // thread function used to communicate with the master

#endif
