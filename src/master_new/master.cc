#include "master.hh"
#include "ecfs.hh"
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <multiset>

constexpr void writetosock (int fd, char * in) {
   char write_buf [BUF_SIZE];
   strncpy (write_buf, in, BUF_SIZE);
   nbwrite (fd, write_buf);
}
// Master() {{{
Master::Master() {
 port_cache      = setted.get<int> ("network.port_mapreduce");
 port_mr         = setted.get<int> ("network.port_cache");
 max_job         = setted.get<int> ("max_job");
 string ipc_path = setted.get<string> ("path.ipc");
 nodelist        = setted.get<vector<string> > ("network.nodes");

 for (auto& node : nodelist) {
   nodes.instert (Node (node));
 }
}
// }}}
// Accept {{{
bool Master::accept () {
  int connectioncount = 0
  while (true) {
   int fd;
   char read_buf [BUF_SIZE];
   struct sockaddr_in connaddr;

   fd = accept (sock_mr, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
   
   writetosock("whoareyou");
   nbread (fd, read_buf);

   if (strncmp (read_buf, "slave", 5) == 0) {
     // get ip address of slave
     char* haddrp = inet_ntoa (connaddr.sin_addr);

     // set fd and max task of connect slave
     for (auto& node : nodes) {
       if (node.get_address() == haddrp) {
         fcntl (fd, F_SETFL, O_NONBLOCK);
         setsockopt (fd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
         setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
         node.fd (fd).max_map_task (MAP_SLOT).max_reduce_task (REDUCE_SLOT);
       }
       connectioncount++;
       break;
     }

     printf ("slave node connected from %s \n", haddrp);
   }
   // Clients comes after, lets keep it simple {{{
   // else if (strncmp (read_buf, "client", 6) == 0)         // client connected
   // {
   //   fcntl (fd, F_SETFL, O_NONBLOCK);
   //   Nodes.insert (Client(fd));

   //   haddrp = inet_ntoa (connaddr.sin_addr);
   //   printf ("a client node connected from %s \n", haddrp);
   // }
   // }}}

   if (connectionscount == nodelist.size()) break;
  }
}
// }}}
// setup_network {{{
bool Master::setup_network () {
  struct sockaddr_in serveraddr;
  // socket open
  sock_mr = socket (AF_INET, SOCK_STREAM, 0);

  if (sock_mr < 0)
  {
    cout << "[master]Socket opening failed" << endl;
  }

  // bind
  int valid = 1;
  memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
  setsockopt (sock_mr, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof (valid));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);
  serveraddr.sin_port = htons ( (unsigned short) port);

  if (bind (sock_mr, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0)
  {
    cout << "[master]\033[0;31mBinding failed\033[0m" << endl;
    exit (1);
  }

  // listen
  if (listen (sock_mr, BACKLOG) < 0)
  {
    cout << "[master]Listening failed" << endl;
    exit (1);
  }
  return true;
}
// }}}
// accept_new_connections {{{
void Master::accept_new_connections () {
  int tmpfd = accept (sock_mr, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
  if (tmpfd <= 0) { perror ("Broken"); }

  writetosock (tmpfd, "whoareyou");

  char read_buf [BUF_SIZE];
  memset (read_buf, 0, BUF_SIZE);
  nbread (tmpfd, read_buf);
  char* haddrp = inet_ntoa (connaddr.sin_addr);

  if (strncmp (read_buf, "client", 6) == 0) {
    printf ("[master]Client node connected from %s \n", haddrp);
    nodes.instert (Client(tmpfd));

  } else if (strncmp (read_buf, "job", 3) == 0) {
    //Skip case of maxjobs
    // reply to the client
    char reply_msg [BUF_SIZE]
    snprintf (reply_msg, BUF_SIZE, "jobid %d", num_jobs);
    writetosock(tmpfd, reply_msg);

    //Find out whether we know that client or not 
    haddrp = inet_ntoa (connaddr.sin_addr);
    auto it = find_if (nodes.begin(), nodes.end(), [haddrp] (Node& n) { 
        return n.address() == haddrp && n.type() == CLIENT; 
        });

    Node& node;
    if (*it == nodes.end())
      node = new Client(tmpfd);

    else 
      node = *it;

    node.set_job (Job(tmpfd))
  }
}
// }}}
// next_message {{{
std::tuple<Node, string> Master::next_message () {
  //Listen for everything in the system
  int fd = -1;
  char read_buf [BUF_SIZE];
  for (auto& node : nodes) {
    fd = node.fd();
    bzero (&read_buf, BUF_SIZE);
    int read_bytes = nbread (fd, read_buf);

    if (read_bytes <= 0) continue; //Default case

    if (node.type == SLAVE) {
      if (strncmp (read_buf, "peerids", 7) == 0) {
        char* token;
        Job* thejob;
        token = strtok (read_buf, " ");   // token <- "peerids"
        token = strtok (NULL, " ");   // token <- jobid
        thejob = find_jobfromid (atoi (token));
        // token first ids
        token = strtok (NULL, " ");

        while (token != NULL) {
          thejob->peerids.insert (atoi (token));
          token = strtok (NULL, " ");   // good
        }

      } else if (strncmp (read_buf, "taskcomplete", 12) == 0) {
        char* token;
        int ajobid, ataskid;
        master_job* thejob;
        master_task* thetask;
        token = strtok (read_buf, " ");   // token <- "taskcomplete"
        token = strtok (NULL, " ");   // token <- "jobid"
        token = strtok (NULL, " ");   // token <- job id
        ajobid = atoi (token);
        token = strtok (NULL, " ");   // token <- "taskid"
        token = strtok (NULL, " ");   // token <- task id
        ataskid = atoi (token);
        thejob = find_jobfromid (ajobid);
        thetask = thejob->find_taskfromid (ataskid);
        thejob->finish_task (thetask, slaves[i]);
        cout << "[master]A task completed(jobid: " << ajobid << ", ";
        cout << thejob->get_numcompleted_tasks();
        cout << "/" << thejob->get_numtasks() << ")" << endl;
      }
    } else if (node.type == CLIENT) {
      if (strncmp(read_buf, "jobconf", 7) == 0) {
      
      } else if (strncmp (read_buf, "complete", 8) == 0) {
      
      }
    }
  }
}
// }}}
// run {{{
bool Master::run() {
  while (mainloop) {
    string query; 
    Client client;

    accept_new_connections();
    std::tie<client, query> = next_message();

    switch (client.type) {
      case SLAVE:  

        break;

        if (client.idle()) {
          for (auto task : client.tasks) {
            switch (task.stage()) {

              case MAP_STAGE:
                switch (task.status()) {

                  case TASK_FINISHED:
                    task << "mapcomplete";
                    cache.flush(task);
                    task.status = REQUEST_SENT;  
                    break;

                  case RESPOND_RECIEVED:
                    task.stage(REDUCE_STAGE);
                    break;
                }
                break;

              case REDUCE_STAGE:
                task << "complete";
                task.stage(COMPLETED_STAGE);

            } 
          }
        }
        break;
        //case OPERATOR: 
        //  // I wont cover this one yet
        //  break;

    }
    schedule_it();
  }
  return true;
}
//}}}
