#include "slave.hh"

#include <iostream>
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
#include <algorithm>

using namespace std;

// Constructor {{{
Slave::Slave() {
  Settings setted; 
  setted.load();

  int port           = setted.get<int>("network.port_mapreduce");
  string master_addr = setted.get<string>("network.master");
  string logname     = setted.get<string> ("log.name");
  string logtype     = setted.get<string> ("log.type");
  logger             = Logger::connect(logname, logtype);

  masterfd = connect_to_server (master_addr.c_str(), port);
}
// }}}
// Destructor {{{
Slave::~Slave() {
  Logger::disconnect(logger);
}
// }}}
// main loop {{{
void Slave::main_loop () {
  while (true) {
    block_until_event (masterfd);    // Block until any of the fd is ready or child signal
    read_master (masterfd);          // Read message from the master node
    read_ipc (masterfd);             // Read from the child process
    check_jobs ();                   // Check if any child proc is dead
    check_tasks (masterfd);          // Check if the task is finished
  }
  close_and_exit (masterfd);
} //}}}
// Connect to Server {{{
int Slave::connect_to_server (const char* host, unsigned short port) {
    struct sockaddr_in serveraddr = {0};
    
    int masterfd = socket (AF_INET, SOCK_STREAM, 0); //SOCK_STREAM -> tcp

    logger->panic_if (masterfd == EXIT_FAILURE, "[slave]Openning socket failed");
    
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons (port);
    inet_pton (AF_INET, host, &serveraddr.sin_addr);

    connect (masterfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr));
    if (masterfd == EXIT_FAILURE)
        logger->panic ("[slave]Connecting to master failed");
    
    // set master socket to be non-blocking socket to avoid deadlock
    fcntl (masterfd, F_SETFL, O_NONBLOCK);
    int buffersize = BUFFERSIZE;
    setsockopt (masterfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
    setsockopt (masterfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
    return masterfd;
}
// }}}
// launch_task {{{
void Slave::launch_task (slave_task& atask) {
    pid_t pid;
    int fd1[2], fd2[2];                  // two set of fds between slave and task(1)
    pipe (fd1);                          // fd1[0]: slave read, fd1[1]: task write
    pipe (fd2);                          // fd2[0]: task read, fd2[1]: slave write

    fcntl (fd1[0], F_SETFL, O_NONBLOCK); // set pipe fds to be non-blocking to avoid deadlock
    fcntl (fd1[1], F_SETFL, O_NONBLOCK);
    fcntl (fd2[0], F_SETFL, O_NONBLOCK);
    fcntl (fd2[1], F_SETFL, O_NONBLOCK);

    atask.set_readfd (fd1[0]).set_writefd (fd2[1]); // set pipe fds
    pid = fork();

    switch (pid) {
      case 0: {
        int count = atask.get_argcount();
        char** args_src = atask.get_argvalues();
        char** args = new char*[count + 4];

        transform (args_src, args_src + count, args, strdup);

        asprintf (&args[count], "%i", fd2[0]);            // pass pipe fds
        asprintf (&args[count + 1], "%i", fd1[1]);
        args[count + 3] = NULL;                           // pass null to last parameter
        
        switch (atask.get_taskrole()) {
          case MAP:    args[count + 2] = strdup("MAP"); break;
          case REDUCE: args[count + 2] = strdup("REDUCE"); break;
          default:     logger->panic ("The role of the task is not defined in %s", __func__);
        }
          
        logger->info ("Launching task with args: %s %s %s %s", args[0], args[1], args[2], args[3]);
        if (execv (args[0], args) == EXIT_FAILURE) // launch the task with the passed arguments
            logger->panic ("execv failed");

      } break;

      case EXIT_FAILURE:
        logger->info ("Task could not have been started due to child process forking failure");
        break;

      default:
        close (fd2[0]); // close pipe fds for task side
        close (fd1[1]);
        atask.set_pid (pid); // register the pid of the task process
    }
} // }}}
// block_until_event {{{
void Slave::block_until_event(int masterfd) {
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
} // }}}
void Slave::read_master (int masterfd) { // {{{
  switch (nbread (masterfd, read_buf)) {
    case 0: 
      logger->error ("[slave]Connection from master is abnormally closed");
      close_and_exit (masterfd);
      break;

    case -1: break;

    default:
      if (strncmp (read_buf, "whoareyou", 9) == 0) {
        snbwrite(masterfd, "slave");

      } else if (strncmp (read_buf, "close", 5) == 0) {
        logger->info ("[slave]Close request from master");
        close_and_exit (masterfd);

      } else if (strncmp (read_buf, "tasksubmit", 10) == 0) { // launch the forwarded task {{{ 
        slave_job* thejob = NULL;
        int jobid, taskid, argc;
        char *token, type [64], remaining[256];

        sscanf (read_buf, "%*s %i %i %s %i %[^\n]", &jobid, &taskid, type, &argc, remaining);

        auto it = find_if (running_jobs.begin(), running_jobs.end(), [jobid] (slave_job* j) -> bool {
              return j->get_jobid() == jobid;
            });

        if (it == running_jobs.end()) {      // if any task in this job are not running in this slave
          thejob = new slave_job (jobid, masterfd);
          running_jobs.push_back (thejob);

        } else {
          thejob = *it;
        }

        slave_task& task = *(new slave_task (taskid));     // the status is running by default
        running_tasks.push_back (&task);                   // add to the running_tasks vector
        thejob->add_task (&task);                          // add this task in 'thejob'

        logger->debug ("Capture task with args: %i %i %s %i %s", jobid, taskid, type, argc, remaining);

        if (strncmp (type, "MAP", 3) == 0)
        {
          task.set_taskrole (MAP) .set_argcount (argc);

          char** values = new char*[argc];

          for (int i = 0; i < argc; i++) {
            token = strtok_r (remaining, " ", &tokenbuf);
            values[i] = strdup (token);
          }
          
          task.set_argvalues (values);

          bool einput_received = false;
          while (not einput_received) { // read messages from master until getting Einput
            switch (nbread (masterfd, read_buf)) {
              case 0:
                logger->error ("[slave]Connection from master is abnormally closed");
                break;

              case -1: continue;
              default:
                if (strncmp (read_buf, "inputpath", 9) == 0) {
                  token = strtok_r (read_buf, " ", &tokenbuf);   // token <- "inputpath"

                  while ( (token = strtok_r(NULL, " ", &tokenbuf)) )
                    task.add_inputpath (token);

                } else if (strncmp (read_buf, "Einput", 6) == 0) { 
                  einput_received = true;
                  break;

               } else {
                  logger->error ("[slave]Unexpected message order from master[%c|%s]",read_buf[0], read_buf);
                }
            }
          }
          launch_task (task); // launch the forwarded task

        } else if (strncmp (type, "REDUCE", 6) == 0) {
          task.set_taskrole (REDUCE) .set_argcount (argc);

          char** values = new char*[argc];
          for (int i = 0; i < argc; i++) {
            token = strtok_r (remaining, " ", &tokenbuf);;
            values[i] = strdup (token);
          }

          task.set_argvalues (values);

          for (int ret = 0; (ret = nbread (masterfd, read_buf)) <= 0; ) { // read messages from master
            logger->error_if (ret == 0, "[slave]Connection from master is abnormally closed");
          }

          string tmp;
          stringstream ss (read_buf);
          ss >> tmp;

          while (ss.good()) {
            ss >> tmp; 
            task.peerids.push_back (stoi(tmp));
            ss >> tmp; 
            task.numiblocks.push_back (stoi(tmp));
          }

         // token = strtok_r (read_buf, " ", &tokenbuf);      // <- "inputpath"
         // token = strtok_r (NULL, " ", &tokenbuf);          // <- first peer id
         // while (token)
         // {
         //   task.peerids.push_back (atoi (token));
         //   token = strtok_r (NULL, " ", &tokenbuf);      // <- numiblock
         //   task.numiblocks.push_back (atoi (token));
         //   token = strtok_r (NULL, " ", &tokenbuf);      // <- peerids
         // }
          launch_task (task);                            // launch the forwarded task

        } else {
          logger->error  ("Debugging: the task role is undefined well.");
          task.set_taskrole (JOB);
        } //}}}
      } else {
        logger->error ("[slave]Undefined signal from master: %s", read_buf);
      }
  } 
} // }}}
void Slave::read_ipc (int masterfd) { //{{{
  for (auto& task : running_tasks) { // check message from tasks through pipe
      switch (nbread (task->get_readfd(), read_buf)) {
        case 0:  break;
        case -1: continue;
        default: 
          if (strncmp (read_buf, "complete", 8) == 0) {
              if (task->get_taskrole() == MAP)        // map task
              {
                  logger->debug("Complete msg from app: %s", read_buf);

                  string to_replace = "peerids " + to_string (task->get_job()->get_jobid());
                  string message = string (read_buf).replace (0, 8, to_replace);
                  snbwrite (masterfd, message);
              }
              
              snbwrite (task->get_writefd(), "terminate");
              task->set_status (COMPLETED); // mark the task as completed

          } else if (strncmp (read_buf, "requestconf", 11) == 0) { // parse all task configure

              string message = "taskconf";
              message += " " + to_string (task->get_job()->get_jobid());
              message += " " + to_string (task->get_taskid());

              snbwrite (task->get_writefd(), message);

              if (task->get_taskrole() == MAP) { // send input paths
                  message = "inputpath";

                  for (int iter = 0; iter < task->get_numinputpaths(); iter++) {
                      if (message.length() + task->get_inputpath (iter).length() + 1 <= BUF_SIZE) {
                          message += " " + string(task->get_inputpath (iter));

                      } else {
                          if (task->get_inputpath (iter).length() + 10 > BUF_SIZE)
                              logger->error ("The length of inputpath excceded the limit");
                          
                          snbwrite (task->get_writefd(), message); // send message to slave
                          message = "inputpath " + string (task->get_inputpath (iter));
                      }
                      logger->debug ("Sending msg to child args: %s", message.c_str());
                  }
                  
                  if (message.length() > strlen ("inputpath ")) // send remaining paths
                      snbwrite (task->get_writefd(), message);
                  
                  snbwrite (task->get_writefd(), "Einput");  // notify end of inputpaths

              } else { // send input paths
                  message = "inputpath";
                  for (int j = 0; (unsigned) j < task->peerids.size(); j++) {
                    message += " " + to_string (task->peerids[j]);
                    message += " " + to_string (task->numiblocks[j]);
                  }
                  snbwrite (task->get_writefd(), message);
              }

          } else {
              logger->error ("[slave]Undefined message protocol from task [Message: %s]", read_buf);
          }
      }
  } 
}//}}}
void Slave::check_jobs() { //{{{
  for (unsigned i = 0; i < running_jobs.size(); i++) {   // check if all tasks in the job are finished

      if (running_jobs[i]->get_numrunningtasks() == 0) { // all task is finished
          slave_job* deleted_job = running_jobs[i];      // clear job from the vectors
          running_jobs.erase (running_jobs.begin() + i);
          i--;
          delete deleted_job;
      }
  } 
}// }}}
// check_tasks {{{
//
void Slave::check_tasks (int masterfd) {
  for (unsigned i = 0; i < running_tasks.size(); i++) {
    if (waitpid (running_tasks[i]->get_pid(), & (running_tasks[i]->pstat), WNOHANG)) {

      if (running_tasks[i]->get_status() == COMPLETED) {     // successful termination
        snprintf (write_buf, BUF_SIZE, "taskcomplete jobid %i taskid %i", 
            running_tasks[i]->get_job()->get_jobid(),
            running_tasks[i]->get_taskid());

        nbwrite (masterfd, write_buf);

        // clear all to things related to this task
        running_tasks[i]->get_job()->finish_task (running_tasks[i]);
        delete running_tasks[i];
        running_tasks.erase (running_tasks.begin() + i);
        i--;

      } else {
        logger->error ("task with taskid %d jobid %d pid: %d terminated abnormally", 
            running_tasks[i]->get_taskid(),
            running_tasks[i]->get_job()->get_jobid(),
            running_tasks[i]->get_pid());

        sleep (1);
        // TODO: clear data structures for the task
        // TODO: launch the failed task again
      }
    }
  }
}  //}}}
// close_and_exit() {{{
void Slave::close_and_exit (int masterfd) { 
  while (close (masterfd) == EXIT_FAILURE) {
    logger->info ("[slave]Closing socket failed");
    usleep (1000);  // sleeps for 1 milli second
  }

  logger->info ("[slave]Exiting slave...");
  exit (EXIT_SUCCESS);
} //}}}
