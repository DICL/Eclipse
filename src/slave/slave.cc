#include "slave.hh"
#include "slave_job.hh"
#include "slave_task.hh"

#include <iostream>
#include <pthread.h>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <common/ecfs.hh>

// mpi
#include <mpi.h>

using namespace std;

char read_buf[BUF_SIZE];
char write_buf[BUF_SIZE];

int masterrank = 0;
bool master_is_set = false;
string localhostname;
vector<slave_job*> running_jobs; // a vector of job, one or multiple tasks of which are running on this slave node
vector<slave_task*> running_tasks; // a vector of running tasks

int main (int argc, char** argv)
{
    // initialize data structures from setup.conf
    Settings setted;
    setted.load_settings();

	// initialize mpi variables
	int myrank, numprocs;
	MPI_Status status;
	MPI_Request request;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    
    // read hostname from hostname file
    ifstream hostfile;
    string hostpath = setted.scratch_path();
    hostpath.append ("hostname");
    hostfile.open (hostpath.c_str());
    hostfile >> localhostname;

	// send message to master
	memset (write_buf, 0, BUF_SIZE);
	strcpy (write_buf, "slave");
	MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, masterrank, 0, MPI_COMM_WORLD);
    
    // set master socket to be non-blocking socket to avoid deadlock
    signal_listener();
    return 0;
}

void signal_listener()
{
    // get signal from master, jobs and tasks
    int readbytes = 0;
    struct timeval time_start;
    struct timeval time_end;
    gettimeofday (&time_start, NULL);
    gettimeofday (&time_end, NULL);
    
    while (1)
    {
		// non-blocking receive
		MPI_Irecv(&read_buf, BUF_SIZE, MPI_CHAR, masterrank, 0, MPI_COMM_WORLD, request);
		MPI_Wait(&request, &status);
		MPI_Get_count(&status, MPI_CHAR, &readbytes);
        
        if (readbytes == 0)     //connection closed from master
        {
            cout << "[slave]Connection from master is abnormally closed" << endl;
            cout << "[slave]Exiting slave..." << endl;
            exit (0);
        }
        else if (readbytes < 0)
        {
            // do nothing
        }
        else     // signal arrived from master
        {
            if (strncmp (read_buf, "close", 5) == 0)
            {
                cout << "[slave]Close request from master" << endl;
                cout << "[slave]Exiting slave..." << endl;
                return;
            }
            else if (strncmp (read_buf, "tasksubmit", 10) == 0)
            {
                // launch the forwarded task
                slave_job* thejob = NULL;
                slave_task* thetask = NULL;
                int jobid;
                int taskid;
                char* token;
                token = strtok (read_buf, " ");   // token <- "tasksubmit"
                token = strtok (NULL, " ");   // token <- jobid
                jobid = atoi (token);
                thejob = find_jobfromid (jobid);
                
                if (thejob == NULL)     // if any task in this job are not running in this slave
                {
                    thejob = new slave_job (jobid, masterfd);
                    running_jobs.push_back (thejob);
                }
                
                token = strtok (NULL, " ");   // token <- taskid
                taskid = atoi (token);
                thetask = new slave_task (taskid);   // the status is running by default
                // add to the running_tasks vector
                running_tasks.push_back (thetask);
                // add this task in 'thejob'
                thejob->add_task (thetask);
                token  = strtok (NULL, " ");   // token <- role
                
                if (strncmp (token, "MAP", 3) == 0)
                {
                    thetask->set_taskrole (MAP);
                    token = strtok (NULL, " ");
                    int argc = atoi (token);
                    thetask->set_argcount (argc);
                    char** values = new char*[argc];
                    
                    for (int i = 0; i < argc; i++)
                    {
                        token = strtok (NULL, " ");;
                        values[i] = new char[strlen (token) + 1];
                        strcpy (values[i], token);
                    }
                    
                    thetask->set_argvalues (values);
                    
                    // read messages from master until getting Einput
                    while (1)
                    {
						MPI_Irecv(&read_buf, BUF_SIZE, MPI_CHAR, masterrank, 0, MPI_COMM_WORLD, request);
						MPI_Wait(&request, &status);
						MPI_Get_count(&status, MPI_CHAR, &readbytes);
                        
                        if (readbytes == 0)
                        {
                            cout << "[slave]Connection from master is abnormally closed" << endl;
                        }
                        else if (readbytes < 0)
                        {
                            continue;
                        }
                        else     // a message
                        {
                            if (strncmp (read_buf, "inputpath", 9) == 0)
                            {
                                token = strtok (read_buf, " ");   // token <- "inputpath"
                                token = strtok (NULL, " ");
                                
                                while (token != NULL)
                                {
                                    // add the input path to the task
                                    thetask->add_inputpath (token);
                                    token = strtok (NULL, " ");
                                }
                            }
                            else if (strncmp (read_buf, "Einput", 6) == 0)
                            {
                                // break the while loop
                                break;
                            }
                            else
                            {
                                cout << "[slave]Unexpected message order from master" << endl;
                            }
                        }
                    }
                    
                    // launch the forwarded task
                    launch_task (thetask);
                }
                else if (strncmp (token, "REDUCE", 6) == 0)
                {
                    thetask->set_taskrole (REDUCE);
                    token = strtok (NULL, " ");
                    int argc = atoi (token);
                    thetask->set_argcount (argc);
                    char** values = new char*[argc];
                    
                    for (int i = 0; i < argc; i++)
                    {
                        token = strtok (NULL, " ");;
                        values[i] = new char[strlen (token) + 1];
                        strcpy (values[i], token);
                    }
                    
                    thetask->set_argvalues (values);
                    
                    // read messages from master
                    while (1)
                    {
						MPI_Irecv(&read_buf, BUF_SIZE, MPI_CHAR, masterrank, 0, MPI_COMM_WORLD, request);
						MPI_Wait(&request, &status);
						MPI_Get_count(&status, MPI_CHAR, &readbytes);
                        
                        if (readbytes == 0)
                        {
                            cout << "[slave]Connection from master is abnormally closed" << endl;
                        }
                        else if (readbytes < 0)
                        {
                            continue;
                        }
                        else     // a message
                        {
                            break;
                        }
                    }
                    
                    token = strtok (read_buf, " ");   // <- "inputpath"
                    token = strtok (NULL, " ");   // <- first peer id
                    
                    while (token != NULL)
                    {
                        thetask->peerids.push_back (atoi (token));
                        token = strtok (NULL, " ");   // <- numiblock
                        thetask->numiblocks.push_back (atoi (token));
                        token = strtok (NULL, " ");   // <- next peerid
                    }
                    
                    // launch the forwarded task
                    launch_task (thetask);
                }
                else
                {
                    cout << "Debugging: the task role is undefined well." << endl;
                    thetask->set_taskrole (JOB);
                }
            }
            else
            {
                cout << "[slave]Undefined signal from master: " << read_buf << endl;
                cout << "[slave]Undefined signal size: " << readbytes << endl;
            }
        }
        
        // check the running_jobs
        for (int i = 0; (unsigned) i < running_jobs.size(); i++)
        {
            // check if all tasks in the job are finished
            if (running_jobs[i]->get_numrunningtasks() == 0)     // all task is finished
            {
                // clear job from the vectors
                slave_job* deleted_job = running_jobs[i];
                running_jobs.erase (running_jobs.begin() + i);
                i--;
                delete deleted_job;
                continue;
            }
        }
        
        // check message from tasks through pipe
        for (int i = 0; (unsigned) i < running_tasks.size(); i++)
        {
			MPI_Irecv(&read_buf, BUF_SIZE, MPI_CHAR, running_tasks[i]->getrank(), 0, MPI_COMM_WORLD, request);
			MPI_Wait(&request, &status);
			MPI_Get_count(&status, MPI_CHAR, &readbytes);
            
            if (readbytes == 0)
            {
                // ignore this case as default
            }
            else if (readbytes < 0)
            {
                continue;
            }
            else
            {
                if (strncmp (read_buf, "complete", 8) == 0)
                {
                    char* token;
                    
                    if (running_tasks[i]->get_taskrole() == MAP)     // map task
                    {
                        string message = "peerids ";
                        stringstream ss;
                        ss << running_tasks[i]->get_job()->get_jobid();
                        // receive peerids
                        token = strtok (read_buf, " ");
                        token = strtok (NULL, " ");   // first token(peer id)
                        
                        while (token != NULL)
                        {
                            ss << " ";
                            ss << atoi (token);
                            token = strtok (NULL, " ");
                        }
                        
                        message.append (ss.str());
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
						MPI_Isend(&write_buf, BUF_SIZE, MPI_CHAR, masterrank, 0, MPI_COMM_WORLD, request);
						MPI_Wait(&request, &status);
                    }
                    
                    //cout<<"[slave]Task with taskid "<<running_tasks[i]->get_taskid();
                    //cout<<" and job id "<<running_tasks[i]->get_job()->get_jobid();
                    //cout<<" completed successfully"<<endl;
                    // send terminate message
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, "terminate");
					MPI_Isend(&write_buf, BUF_SIZE, MPI_CHAR, running_tasks[i]->getrank(), 0, MPI_COMM_WORLD, request);
					MPI_Wait(&request, &status);
                    // mark the task as completed
                    running_tasks[i]->set_status (COMPLETED);
                }
                else if (strncmp (read_buf, "requestconf", 11) == 0)
                {
                    // parse all task configure
                    string message;
                    stringstream ss;
                    ss << "taskconf ";
                    // job id
                    ss << running_tasks[i]->get_job()->get_jobid();
                    // task id
                    ss << " ";
                    ss << running_tasks[i]->get_taskid();
                    message = ss.str();
                    // send message to the task
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, message.c_str());
					MPI_Isend(&write_buf, BUF_SIZE, MPI_CHAR, running_tasks[i]->getrank(), 0, MPI_COMM_WORLD, request);
					MPI_Wait(&request, &status);
                    
                    if (running_tasks[i]->get_taskrole() == MAP)
                    {
                        // send input paths
                        message = "inputpath";
                        int iter = 0;
                        
                        while (iter < running_tasks[i]->get_numinputpaths())
                        {
                            if (message.length() + running_tasks[i]->get_inputpath (iter).length() + 1 <= BUF_SIZE)
                            {
                                message.append (" ");
                                message.append (running_tasks[i]->get_inputpath (iter));
                            }
                            else
                            {
                                if (running_tasks[i]->get_inputpath (iter).length() + 10 > BUF_SIZE)
                                {
                                    cout << "[master]The length of inputpath excceded the limit" << endl;
                                }
                                
                                // send message to slave
                                memset (write_buf, 0, BUF_SIZE);
                                strcpy (write_buf, message.c_str());
								MPI_Isend(&write_buf, BUF_SIZE, MPI_CHAR, running_tasks[i]->getrank(), 0, MPI_COMM_WORLD, request);
								MPI_Wait(&request, &status);
                                message = "inputpath ";
                                message.append (running_tasks[i]->get_inputpath (iter));
                            }
                            
                            iter++;
                        }
                        
                        // send remaining paths
                        if (message.length() > strlen ("inputpath "))
                        {
                            memset (write_buf, 0, BUF_SIZE);
                            strcpy (write_buf, message.c_str());
							MPI_Isend(&write_buf, BUF_SIZE, MPI_CHAR, running_tasks[i]->getrank(), 0, MPI_COMM_WORLD, request);
							MPI_Wait(&request, &status);
                        }
                        
                        // notify end of inputpaths
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, "Einput");
						MPI_Isend(&write_buf, BUF_SIZE, MPI_CHAR, running_tasks[i]->getrank(), 0, MPI_COMM_WORLD, request);
						MPI_Wait(&request, &status);
                    }
                    else
                    {
                        // send input paths
                        message = "inputpath";
                        stringstream ss;
                        
                        for (int j = 0; (unsigned) j < running_tasks[i]->peerids.size(); j++)
                        {
                            ss << " ";
                            ss << running_tasks[i]->peerids[j];
                            ss << " ";
                            ss << running_tasks[i]->numiblocks[j];
                        }
                        
                        message.append (ss.str());
                        // notify end of inputpaths
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
						MPI_Isend(&write_buf, BUF_SIZE, MPI_CHAR, running_tasks[i]->getrank(), 0, MPI_COMM_WORLD, request);
						MPI_Wait(&request, &status);
                    }
                }
                else
                {
                    cout << "[slave]Undefined message protocol from task" << endl;
                    cout << "       Message: " << read_buf << endl;
                }
            }
        }
        
        // check task clear
        for (int i = 0; (unsigned) i < running_tasks.size(); i++)
        {
            if (waitpid (running_tasks[i]->get_pid(), & (running_tasks[i]->pstat), WNOHANG))         // waitpid returned nonzero
            {
                if (running_tasks[i]->get_status() == COMPLETED)     // successful termination
                {
                    // send 'taskcomplete' message to the master
                    stringstream ss;
                    string msg = "taskcomplete";
                    ss << " jobid ";
                    ss << running_tasks[i]->get_job()->get_jobid();
                    ss << " taskid ";
                    ss << running_tasks[i]->get_taskid();
                    msg.append (ss.str());
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, msg.c_str());
					MPI_Isend(&write_buf, BUF_SIZE, MPI_CHAR, masterrank, 0, MPI_COMM_WORLD, request);
					MPI_Wait(&request, &status);
                    // clear all to things related to this task
                    running_tasks[i]->get_job()->finish_task (running_tasks[i]);
                    delete running_tasks[i];
                    running_tasks.erase (running_tasks.begin() + i);
                    i--;
                }
                else
                {
                    cout << "[slave]A ";
                    
                    if (running_tasks[i]->get_taskrole() == MAP)
                    {
                        cout << "map ";
                    }
                    else if (running_tasks[i]->get_taskrole() == REDUCE)
                    {
                        cout << "reduce ";
                    }
                    
                    cout << "task with taskid " << running_tasks[i]->get_taskid();
                    cout << " and jobid " << running_tasks[i]->get_job()->get_jobid();
                    cout << " terminated abnormally" << endl;
                    cout << "pid: " << running_tasks[i]->get_pid() << endl;
                    sleep (1);
                    // TODO: clear data structures for the task
                    // TODO: launch the failed task again
                }
            }
        }
        
        gettimeofday (&time_end, NULL);
        
        if (time_end.tv_sec - time_start.tv_sec > 20.0)
        {
            //cout<<"[Slave Heartbeat]";
            //cout<<"numjob: "<<running_jobs.size()<<", ";
            //cout<<"numtask: "<<running_tasks.size()<<"("<<localhostname<<")"<<endl;
            //gettimeofday(&time_start, NULL);
        }
    }
    
    while (close (masterfd) < 0)
    {
        cout << "[slave]Close failed" << endl;
        // sleeps for 1 milliseconds
        usleep (1000);
    }
    
    cout << "[slave]Exiting slave..." << endl;
    exit (0);
}

void launch_task (slave_task* atask)
{
    int pid;
    int fd1[2]; // two set of fds between slave and task(1)
    int fd2[2]; // two set of fds between slave and task(2)
    pipe (fd1);   // fd1[0]: slave read, fd1[1]: task write
    pipe (fd2);   // fd2[0]: task read, fd2[1]: slave write
    // set pipe fds to be non-blocking to avoid deadlock
    fcntl (fd1[0], F_SETFL, O_NONBLOCK);
    fcntl (fd1[1], F_SETFL, O_NONBLOCK);
    fcntl (fd2[0], F_SETFL, O_NONBLOCK);
    fcntl (fd2[1], F_SETFL, O_NONBLOCK);
    // set pipe fds
    atask->set_readfd (fd1[0]);
    atask->set_writefd (fd2[1]);
    pid = fork();
    
    if (pid == 0)     // child side
    {
        // pass all arguments
        char** args;
        int count;
        stringstream ss;
        stringstream ss1;
        stringstream ss2;
        // origianl arguments + write id + pipe fds + task type
        count = atask->get_argcount();
        args = new char*[count + 4];
        
        // pass original arguments
        for (int i = 0; i < count; i++)
        {
            args[i] = new char[strlen (atask->get_argvalues() [i]) + 1];
            strcpy (args[i], atask->get_argvalues() [i]);
            args[i][strlen (atask->get_argvalues() [i])] = 0;
        }
        
        // pass pipe fds
        ss1 << fd2[0];
        ss2 << fd1[1];
        args[count] = new char[ss1.str().length() + 1];
        args[count + 1] = new char[ss2.str().length() + 1];
        strcpy (args[count], ss1.str().c_str());
        strcpy (args[count + 1], ss2.str().c_str());
        args[count][ss1.str().length()] = 0;
        args[count + 1][ss2.str().length()] = 0;
        
        // pass task type
        if (atask->get_taskrole() == MAP)
        {
            args[count + 2] = new char[4];
            strcpy (args[count + 2], "MAP");
            args[count + 2][3] = 0;
            //args[count+2] = "MAP";
        }
        else if (atask->get_taskrole() == REDUCE)
        {
            args[count + 2] = new char[7];
            strcpy (args[count + 2], "REDUCE");
            args[count + 2][6] = 0;
            //args[count+2] = "REDUCE";
        }
        else
        {
            cout << "[slave]Debugging: the role of the task is not defined in launch_task() function" << endl;
            args[count + 2] = new char[4];
            strcpy (args[count + 2], "JOB");
            args[count + 2][3] = 0;
            //args[count+2] = "JOB";
        }
        
        // pass null to last parameter
        args[count + 3] = NULL;
        
        // launch the task with the passed arguments
        while (execv (args[0], args) == -1)
        {
            cout << "Debugging: execv failed" << endl;
            cout << "Arguments:";
            
            for (int i = 0; i < count + 3; i++)
            {
                cout << " " << args[i];
            }
            
            cout << endl;
            // sleeps for 1 seconds and retry execv. change this if necessary
            sleep (1);
        }
    }
    else if (pid < 0)
    {
        cout << "[slave]Task could not have been started due to child process forking failure" << endl;
    }
    else     // parent side
    {
        // close pipe fds for task side
        close (fd2[0]);
        close (fd1[1]);
        // register the pid of the task process
        atask->set_pid (pid);
        // print the launch message
        //cout<<"[slave]A ";
        //if(atask->get_taskrole() == MAP)
        //  cout<<"map ";
        //else if(atask->get_taskrole() == REDUCE)
        //  cout<<"reduce ";
        //cout<<"task launched with taskid "<<atask->get_taskid()<<" and jobid "<<atask->get_job()->get_jobid();
        //cout<<endl;
        return;
    }
}

slave_job* find_jobfromid (int id)
{
    for (int i = 0; (unsigned) i < running_jobs.size(); i++)
    {
        if (running_jobs[i]->get_jobid() == id)
        {
            return running_jobs[i];
        }
    }
    
    return NULL;
}
