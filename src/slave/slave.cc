#include "slave.hh"
#include "slave_job.hh"
#include "slave_task.hh"
#include "../cache_slave/cache_slave.hh"
//#include "../common/ecfs.hh"

#include <iostream>
#include <thread>
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

using namespace std;

char read_buf[BUF_SIZE];
char write_buf[BUF_SIZE];

int port = -1;
int masterfd = -1;

int buffersize = 8388608; // 8 MB buffer size
Logger* log;

string localhostname;
vector<slave_job*> running_jobs; // a vector of job, one or multiple tasks of which are running on this slave node
vector<slave_task*> running_tasks; // a vector of running tasks

void block_until_event();

int main (int argc, char** argv)
{
    Settings setted;
    setted.load();
    port = setted.get<int>("network.port_mapreduce");
    string master_addr = setted.get<string>("network.master");
    localhostname      = setted.getip();
    string logname     = setted.get<string> ("log.name");
    string logtype     = setted.get<string> ("log.type");
    log                = Logger::connect(logname, logtype);

    connect_to_server (master_addr.c_str(), port); // connect to master
   
    auto cache_thread = std::thread ([&] () {
        sleep (5);
        Cache_slave cache_slave;
        cache_slave.connect ();
        cache_slave.run_server (); 
    });

    signal_listener();

    cache_thread.join();
    Logger::disconnect(log);

    return EXIT_SUCCESS;
}

void connect_to_server (const char* host, unsigned short port)
{
    struct sockaddr_in serveraddr = {0};
    struct hostent* hp;
    
    masterfd = socket (AF_INET, SOCK_STREAM, 0); //SOCK_STREAM -> tcp
    
    if (masterfd < 0) {
        log->info ("[slave]Openning socket failed");
        exit (EXIT_FAILURE);
    }
    
    hp = gethostbyname (host);
    if (hp == NULL) {
        log->info ("[slave]Cannot find host by host name");
    }
    
    serveraddr.sin_family = AF_INET;
    memcpy (&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
    serveraddr.sin_port = htons (port);

    connect (masterfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr));
    if (masterfd < 0) {
        log->info ("[slave]Connecting to master failed");
        exit (EXIT_FAILURE);
    }
    
    // set master socket to be non-blocking socket to avoid deadlock
    fcntl (masterfd, F_SETFL, O_NONBLOCK);
    setsockopt (masterfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
    setsockopt (masterfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
}

// get signal from master, jobs and tasks
void signal_listener()
{
    int readbytes = 0;
    struct timeval time_start;
    struct timeval time_end;
    gettimeofday (&time_start, NULL);
    gettimeofday (&time_end, NULL);
    
    while (1)
    {
        block_until_event();
        readbytes = nbread (masterfd, read_buf);
        
        if (readbytes == 0)     //connection closed from master
        {
            log->info ("[slave]Connection from master is abnormally closed");
            
            while (close (masterfd) < 0)
            {
                log->info ("[slave]Closing socket failed");
                usleep (1000);  // sleeps for 1 milli second
            }
            
            log->info ("[slave]Exiting slave...");
            exit (0);
        }
        else if (readbytes < 0)
        {
            // do nothing
        }
        else     // signal arrived from master
        {
            if (strncmp (read_buf, "whoareyou", 9) == 0)
            {
                memset (write_buf, 0, BUF_SIZE);
                strcpy (write_buf, "slave");
                nbwrite (masterfd, write_buf);
            }
            else if (strncmp (read_buf, "close", 5) == 0)
            {
                log->info ("[slave]Close request from master");
                
                while (close (masterfd) < 0)
                {
                    log->info ("[slave]Close failed");
                    usleep (1000); // sleeps for 1 milli seconds
                }
                
                log->info ("[slave]Exiting slave...");
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
                        readbytes = nbread (masterfd, read_buf);
                        
                        if (readbytes == 0)
                        {
                            log->info ("[slave]Connection from master is abnormally closed");
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
                                log->info ("[slave]Unexpected message order from master");
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
                        readbytes = nbread (masterfd, read_buf);
                        
                        if (readbytes == 0)
                        {
                            log->info ("[slave]Connection from master is abnormally closed");
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
                    log->info ("Debugging: the task role is undefined well.");
                    thetask->set_taskrole (JOB);
                }
            }
            else
            {
                log->info ("[slave]Undefined signal from master: %s", read_buf);
                log->info ("[slave]Undefined signal size: %d", readbytes);
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
            readbytes = nbread (running_tasks[i]->get_readfd(), read_buf);
            
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
                        nbwrite (masterfd, write_buf);
                    }
                    
                    // send terminate message
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, "terminate");
                    nbwrite (running_tasks[i]->get_writefd(), write_buf);
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
                    nbwrite (running_tasks[i]->get_writefd(), write_buf);
                    
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
                                    log->info ("[master]The length of inputpath excceded the limit");
                                }
                                
                                // send message to slave
                                memset (write_buf, 0, BUF_SIZE);
                                strcpy (write_buf, message.c_str());
                                nbwrite (running_tasks[i]->get_writefd(), write_buf);
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
                            nbwrite (running_tasks[i]->get_writefd(), write_buf);
                        }
                        
                        // notify end of inputpaths
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, "Einput");
                        nbwrite (running_tasks[i]->get_writefd(), write_buf);
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
                        nbwrite (running_tasks[i]->get_writefd(), write_buf);
                    }
                }
                else
                {
                    log->info ("[slave]Undefined message protocol from task [Message: %s]", read_buf);
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
                    snprintf (write_buf, BUF_SIZE, "taskcomplete jobid %i taskid %i", 
                      running_tasks[i]->get_job()->get_jobid(),
                      running_tasks[i]->get_taskid());

                    nbwrite (masterfd, write_buf);

                    // clear all to things related to this task
                    running_tasks[i]->get_job()->finish_task (running_tasks[i]);
                    delete running_tasks[i];
                    running_tasks.erase (running_tasks.begin() + i);
                    i--;
                    
//                    stringstream ss;
//                    string msg = "taskcomplete";
//                    ss << " jobid ";
//                    ss << running_tasks[i]->get_job()->get_jobid();
//                    ss << " taskid ";
//                    ss << running_tasks[i]->get_taskid();
//                    msg.append (ss.str());
//                    memset (write_buf, 0, BUF_SIZE);
//                    strcpy (write_buf, msg.c_str());
//                    nbwrite (masterfd, write_buf);
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
                    
                    log->info ("task with taskid %d ", running_tasks[i]->get_taskid());
                    log->info ("and jobid %d ", running_tasks[i]->get_job()->get_jobid());
                    log->info (" terminated abnormally");
                    log->info ("pid: %d", running_tasks[i]->get_pid());
                    sleep (1);
                    // TODO: clear data structures for the task
                    // TODO: launch the failed task again
                }
            }
        }
        gettimeofday (&time_end, NULL);
    }
    
    while (close (masterfd) == EXIT_FAILURE)
    {
        log->info ("[slave]Close failed");
        usleep (1000); // sleeps for 1 milliseconds
    }
    log->info ("[slave]Exiting slave...");
}

void launch_task (slave_task* atask)
{
    pid_t pid;
    int fd1[2], fd2[2];                  // two set of fds between slave and task(1)
    pipe (fd1);                          // fd1[0]: slave read, fd1[1]: task write
    pipe (fd2);                          // fd2[0]: task read, fd2[1]: slave write

    fcntl (fd1[0], F_SETFL, O_NONBLOCK); // set pipe fds to be non-blocking to avoid deadlock
    fcntl (fd1[1], F_SETFL, O_NONBLOCK);
    fcntl (fd2[0], F_SETFL, O_NONBLOCK);
    fcntl (fd2[1], F_SETFL, O_NONBLOCK);

    atask->set_readfd (fd1[0]);          // set pipe fds
    atask->set_writefd (fd2[1]);

    pid = fork();
    switch (pid) {
      case 0: {
        int count = atask->get_argcount();
        char** args = new char*[count + 4];
        
        for (int i = 0; i < count; i++)                        // pass original arguments
            args[i] = strdup (atask->get_argvalues()[i]);
        
        args[count]     = strdup (to_string (fd2[0]).c_str()); // pass pipe fds
        args[count + 1] = strdup (to_string (fd1[1]).c_str()); //
        args[count + 3] = NULL;                                // pass null to last parameter
        
        switch (atask->get_taskrole()) {
          case MAP:    args[count + 2] = strdup("MAP"); break;
          case REDUCE: args[count + 2] = strdup("REDUCE"); break;
          default:     log->panic ("The role of the task is not defined in %s", __func__);
        }

        if (execv (args[0], args) == EXIT_FAILURE) // launch the task with the passed arguments
            log->panic ("execv failed");

      } break;
      case EXIT_FAILURE:
        log->info ("Task could not have been started due to child process forking failure");
        break;

      default:
        close (fd2[0]); // close pipe fds for task side
        close (fd1[1]);
        atask->set_pid (pid); // register the pid of the task process
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

void block_until_event() {
  struct timespec ts;
  ts.tv_sec  = 0;
  ts.tv_nsec = 10000000;

  sigset_t mask;
  sigemptyset (&mask);
  sigaddset (&mask, SIGCHLD);

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(masterfd, &fds); /* adds sock to the file descriptor set */

  for (auto& ijob : running_tasks)
    FD_SET(ijob->get_readfd(), &fds);

  pselect (masterfd+running_tasks.size(), &fds, NULL, NULL, &ts, &mask);
}
