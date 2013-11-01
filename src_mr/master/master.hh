#ifndef _MASTER_
#define _MASTER_

#include <iostream>
#include <string>
#include <mapreduce/job.hh>


int open_server(int port); // function which receive connections from slaves
void *accept_client(void *args); // thread function used to receive connections from clients
void *signal_listener(void *args); // thread function used to communicate with connected nodes
void run_job(char* buf_content, job* thejob); // run submitted job

#endif
