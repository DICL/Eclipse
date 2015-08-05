#ifndef __MASTER__
#define __MASTER__

#include <iostream>
#include <string>
#include "master_job.hh"

// master functions
void open_server (int port);   // function which receive connections from slaves
void *accept_client (void *args);   // thread function used to receive connections from clients
void run_job (char* buf_content, master_job* thejob);   // run submitted job
master_job* find_jobfromid (int id);   // find and return job pointer fro its jobid
void launch_slave (int rank);
void mpi_mater_listener (int rank);
void socket_listener (int fd);

// cache server functions
void handle_boundaries(char*);
void handle_iwritefinish(char*);
void check_iwriterequest(char *);
void check_cache_slaves ();

#endif
