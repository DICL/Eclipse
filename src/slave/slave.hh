#ifndef __SLAVE_HH__
#define __SLAVE_HH__

#ifndef BUFFERSIZE 
#define BUFFERSIZE 8388608
#endif 

#include "slave_task.hh"
#include "slave_job.hh"
#include "../common/logger.hh"
#include "string"

class Slave {
  public:
    void main_loop();
    Slave();
    ~Slave();

  private:
    int connect_to_server (const char*, unsigned short int);   // function which connect to the master
    void block_until_event (int);
    void read_master (int); 
    void check_jobs ();
    void check_tasks (int);
    void read_ipc (int);
    void close_and_exit (int);
    void launch_task (slave_task& atask);   // launch forwarded task
    slave_job* find_jobfromid (int id);   // return the slave_job with input id if it exist

    char read_buf[BUF_SIZE], write_buf[BUF_SIZE];
    char* tokenbuf;
    int masterfd;
    Logger* logger;
    vector<slave_job*> running_jobs;
    vector<slave_task*> running_tasks;
};

#endif
