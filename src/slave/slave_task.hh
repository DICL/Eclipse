#ifndef _SLAVE_TASK_
#define _SLAVE_TASK_

#include <iostream>
#include <common/ecfs.hh>
#include <vector>
#include <string>

using namespace std;

class slave_job;

class slave_task
{
    private:
        int taskid;
        int pid;
        mr_role role; // MAP or REDUCE
        task_status status;
        int pipefds[2]; // 0 for read, 1 for write
        int argcount;
        char** argvalues;
        slave_job* job;
        vector<string> inputpaths; // a vector of inputpaths. inputpaths can be multiple
        
    public:
        vector<int> peerids;
        vector<int> numiblocks;
        slave_task();
        slave_task (int id);
        ~slave_task();
        
        int pstat; // status value for waitpid()
        
        int get_taskid();
        int get_pid();
        mr_role get_taskrole();
        task_status get_status();
        int get_readfd();
        int get_writefd();
        int get_argcount();
        char** get_argvalues();
        slave_job* get_job();
        string get_inputpath (int index);
        int get_numinputpaths();

        slave_task& set_taskid (int id);
        slave_task& set_pid (int id);
        slave_task& set_taskrole (mr_role arole);
        slave_task& set_status (task_status astatus);
        slave_task& set_readfd (int fd);
        slave_task& set_writefd (int fd);
        slave_task& set_argcount (int num);
        slave_task& set_argvalues (char** argv);
        slave_task& set_job (slave_job* ajob);
        slave_task& add_inputpath (string apath);
};

#endif
