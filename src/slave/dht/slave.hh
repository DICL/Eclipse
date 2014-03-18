#ifndef _SLAVE_
#define _SLAVE_

#include "../slave_task.hh"

int connect_to_server(char *host, unsigned short port); // function which connect to the master
void signal_listener(void); // function used to communicate with the master
void launch_task(slave_task* atask); // launch forwarded task
slave_job* find_jobfromid(int id); // return the slave_job with input id if it exist

#endif
