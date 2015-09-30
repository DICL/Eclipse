#include "slave_job.hh"

using namespace std;

slave_job::slave_job() {
    this->jobid = -1;
    masterfd = -1;
}

slave_job::slave_job (int id, int fd) {
    this->jobid = id;
    masterfd = fd;
}

slave_job::~slave_job() { }

void slave_job::finish_task (slave_task* atask) {
    for (int i = 0; (unsigned) i < this->running_tasks.size(); i++)
    {
        if (this->running_tasks[i] == atask)
        {
            this->running_tasks.erase (this->running_tasks.begin() + i);
            this->completed_tasks.push_back (atask);
            atask->set_status (COMPLETED);
            return;
        }
    }
}

slave_task* slave_job::find_taskfromid (int id) {
    for (int i = 0; (unsigned) i < this->tasks.size(); i++)
    {
        if (this->tasks[i]->get_taskid() == id)
        {
            return this->tasks[i];
        }
    }
    
    cout << "No such a task with that index in this job assigned to this slave" << endl;
    return NULL;
}

void slave_job::set_jobid (int id)     { this->jobid = id; }
int slave_job::get_jobid()             { return this->jobid; }
int slave_job::get_numtasks()          { return this->tasks.size(); }
int slave_job::get_numrunningtasks()   { return this->running_tasks.size(); }
int slave_job::get_numcompletedtasks() { return this->completed_tasks.size(); }

void slave_job::add_task (slave_task* atask) {
    if (atask->get_status() != RUNNING)
    {
        cout << "Debugging: The added task is not at running state." << endl;
    }
    
    atask->set_job (this);
    this->tasks.push_back (atask);
    this->running_tasks.push_back (atask);   // tasks are in running state initially
}

slave_task* slave_job::get_task (int index) {
    if ( (unsigned) index >= this->tasks.size())
    {
        cout << "Debugging: index of bound in the slave_job::get_task() function." << endl;
        return NULL;
    }
    else
    {
        return this->tasks[index];
    }
}

slave_task* slave_job::get_completedtask (int index) {
    if ( (unsigned) index < completed_tasks.size())
    {
        return completed_tasks[index];
    }
    else
    {
        cout << "Debugging: Index out of range in the slave_job::get_completedtask() function" << endl;
        return NULL;
    }
}
