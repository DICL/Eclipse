#include "slave_task.hh"

// Constructors & destructors {{{
slave_task::slave_task() {
    this->taskid = -1;
    this->role = JOB; // this should be changed to MAP or REDUCE
    this->status = RUNNING; // the default is RUNNING because here is slave side
    this->pipefds[0] = -1;
    this->pipefds[1] = -1;
    this->argcount = -1;
    this->argvalues = NULL;
    this-> job = NULL;
    this->pid = 0;
}

slave_task::slave_task (int id) {
    this->taskid = id;
    this->role = JOB; // this should be changed to MAP or REDUCE
    this->status = RUNNING; // the default is RUNNING because here is slave side
    this->pipefds[0] = -1;
    this->pipefds[1] = -1;
    this->argcount = -1;
    this->argvalues = NULL;
    this-> job = NULL;
    this->pid = 0;
}

slave_task::~slave_task()
{
    // close all pipe fds
    close (this->pipefds[0]);
    close (this->pipefds[1]);

    // delete argvalues
    if (argvalues != NULL)
    {
        for (int i = 0; i < this->argcount; i++)
        {
            delete[] argvalues[i];
        }

        delete[] argvalues;
    }
}
// }}}
// Getters {{{
int         slave_task::get_taskid()        { return this->taskid; }
int         slave_task::get_pid()           { return this->pid; }
mr_role     slave_task::get_taskrole()      { return this->role; }
task_status slave_task::get_status()        { return this->status; }
int         slave_task::get_readfd()        { return this->pipefds[0]; }
int         slave_task::get_writefd()       { return this->pipefds[1]; }
int         slave_task::get_argcount()      { return this->argcount; }
char**      slave_task::get_argvalues()     { return this->argvalues; }
slave_job*  slave_task::get_job()           { return this->job; }
int         slave_task::get_numinputpaths() { return this->inputpaths.size(); }

string slave_task::get_inputpath (int index) {
    if ( (unsigned) index >= inputpaths.size())
    {
        cout << "Index out of bound in the slave_task::get_inputpath() function." << endl;
        return "";
    }
    else
    {
        return inputpaths[index];
    }
}
// }}}
// Setters {{{
slave_task& slave_task::set_readfd (int fd)              { this->pipefds[0] = fd; return *this; }
slave_task& slave_task::set_writefd (int fd)             { this->pipefds[1] = fd; return *this; }
slave_task& slave_task::set_job (slave_job* ajob)        { this->job = ajob; return *this; }
slave_task& slave_task::add_inputpath (string apath)     { inputpaths.push_back (apath); return *this; }
slave_task& slave_task::set_argvalues (char** argv)      { this->argvalues = argv; return *this; }
slave_task& slave_task::set_status (task_status astatus) { this->status = astatus; return *this; }
slave_task& slave_task::set_taskid (int id)              { this->taskid = id; return *this; }
slave_task& slave_task::set_pid (int id)                 { this->pid = id; return *this; }
slave_task& slave_task::set_taskrole (mr_role arole)     { this->role = arole; return *this; }
slave_task& slave_task::set_argcount (int num)           { this->argcount = num; return *this; }
// }}}
