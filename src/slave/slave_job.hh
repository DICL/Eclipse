#ifndef _SLAVE_JOB_
#define _SLAVE_JOB_

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <common/ecfs.hh>
#include "slave_task.hh"

class slave_job {
  private:
    int jobid;
    int masterfd;
    std::vector<slave_task*> tasks;
    std::vector<slave_task*> running_tasks;
    std::vector<slave_task*> completed_tasks;

  public:
    slave_job();
    slave_job (int id, int fd);
    ~slave_job();

    void set_jobid (int id);
    int get_jobid();
    void finish_task (slave_task* atask);
    slave_task* find_taskfromid (int id);
    int get_numtasks(); // tasks of this job assigned to this slave
    int get_numrunningtasks(); // running tasks of this job assigned to this slave
    int get_numcompletedtasks(); // completed tasks of this job assigned to this slave
    slave_task* get_completedtask (int index);
    void add_task (slave_task* atask);
    slave_task* get_task (int index);
};

#endif
