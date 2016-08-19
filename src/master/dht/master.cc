#include "master.hh"
#include <iostream>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/unistd.h>
#include <sys/un.h>
#include <common/msgaggregator.hh>
#include <common/hash.hh>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <mapreduce/definitions.hh>
#include <common/settings.hh>
#include <orthrus/histogram.hh>
#include "../master_job.hh"
#include "../master_task.hh"
#include "../connslave.hh"
#include "../connclient.hh"
#include <unordered_map>
#include <list>
#include <sys/mman.h>


// Available scheduling: {FCFS, EMKDE, DELAY}
#define EMKDE // scheduler
// #define DELAY // scheduler
//#define NEW_SCHED // scheduler
// #define LCA
//#define NEW_LCA
// #define ACS

using namespace std;

vector<connslave*> slaves;
vector<connclient*> clients;

vector<master_job*> jobs;

histogram* thehistogram;

int serverfd;
//int cacheserverfd;
int ipcfd; // fd of cacheserver
int port = -1;
int dhtport = -1;
int max_job = -1;
int jobidclock = 0; // job id starts 0

// the last scheduled job index: for fairness across job
int scheduledjob;
bool scheduled;

bool thread_continue;

vector<string> nodelist;


char *read_buf;  // read buffer for signal_listener thread
char *write_buf;  // write buffer for signal_listener thread

//DHTserver* dhtserver; // dht server object
//DHTclient* dhtclient; // dht client object

int slave_iter = 0;

bool jobComp(master_job* a, master_job* b){
	return a->get_numrunning_tasks() < b->get_numrunning_tasks();
}

bool jobCompLatency(master_job* a, master_job* b){
  if (a->get_priority() < b->get_priority()) {
    return true;
  } else if (a->get_priority() == b->get_priority()) {
    return a->get_numrunning_tasks() < b->get_numrunning_tasks();
  } else {
    return false;
  }
}


string SetInputPathMessage (master_task *thetask);

// --------------------------master protocol---------------------------------
// 1. whoareyou: send a message to identify the connected node
// 2. close: let the destination node close the connection from the master
// 3. ignored: let connected node know message from it was ignored
// 4. result: contains resulting messeage to client followed by 'result:'
// --------------------------------------------------------------------------
// TODO: make protocols to integer or enum

int main (int argc, char** argv)
{
  read_buf = (char*)malloc(BUF_SIZE);
  write_buf = (char*)malloc(BUF_SIZE);
	// initialize data structures from setup.conf
	ifstream conf;
	string token;
	Settings setted;
	setted.load_settings();

	port = setted.port();
	dhtport = setted.dhtport();
	max_job = setted.max_job();

	// verify initialization
	if (port == -1)
	{
		cout << "[master]port should be specified in the setup.conf" << endl;
		exit (1);
	}

	if (max_job == -1)
	{
		cout << "[master]max_job should be specified in the setup.conf" << endl;
		exit (1);
	}

	nodelist = setted.nodelist();

	for (int i = 0; (unsigned) i < nodelist.size(); i++)
	{
		slaves.push_back (new connslave (nodelist[i]));
	}

	// initialize the EM-KDE histogram
	thehistogram = new histogram (nodelist.size(), NUMBIN);
	open_server (port);

	if (serverfd < 0)
	{
		cout << "[master]\033[0;31mOpenning server failed\033[0m" << endl;
		return 1;
	}

	if (ipcfd < 0)
	{
		cout << "[master]\033[0;31mOpenning server failed\033[0m" << endl;
		return 1;
	}

	struct sockaddr_in connaddr;

	int addrlen = sizeof (connaddr);

	char* haddrp;

	int connectioncount = 0;

	int buffersize = 8388608; // 8 MB buffer

	while (1)     // receives connection from slaves
	{
		int fd;
		fd = accept (serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);

		if (fd < 0)
		{
			cout << "[master]Accepting failed" << endl;
			// sleep 0.0001 seconds. change this if necessary
			usleep (1000);
			continue;
		}
		else
		{
			// check if the accepted node is slave or client
			memset (write_buf, 0, BUF_SIZE);
			strcpy (write_buf, "whoareyou");
			nbwrite (fd, write_buf);
			// this is a blocking read
			// so don't need to check the transfer completion
			nbread (fd, read_buf);

			if (strncmp (read_buf, "slave", 5) == 0)       // slave connected
			{
				// get ip address of slave
				haddrp = inet_ntoa (connaddr.sin_addr);

				// set fd and max task of connect slave
				for (int i = 0; (unsigned) i < slaves.size(); i++)
				{
					if (slaves[i]->get_address() == haddrp)
					{
						slaves[i]->setfd (fd);
						slaves[i]->setmaxmaptask (MAP_SLOT);
						slaves[i]->setmaxreducetask (REDUCE_SLOT);
						fcntl (fd, F_SETFL, O_NONBLOCK);
						setsockopt (fd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
						setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
						connectioncount++;
						break;
					}
				}

				printf ("[master]A slave node connected from %s \n", haddrp);
			}
			else if (strncmp (read_buf, "client", 6) == 0)         // client connected
			{
				clients.push_back (new connclient (fd));
				// set sockets to be non-blocking socket to avoid deadlock
				fcntl (clients.back()->getfd(), F_SETFL, O_NONBLOCK);
				// get ip address of client
				haddrp = inet_ntoa (connaddr.sin_addr);
				printf ("[master]A client node connected from %s \n", haddrp);
			}
			else if (strncmp (read_buf, "job", 3) == 0)
			{
				// TODO: deal with the case that a job joined the
				// server before all slave connected
			}
			else     // unknown connection
			{
				// TODO: deal with this case
				cout << "[master]Unknown connection" << endl;
			}

			// break if all slaves are connected
			if ( (unsigned) connectioncount == nodelist.size())
			{
				cout << "[master]\033[0;32mAll slave nodes are connected successfully\033[0m" << endl;
				break;
			}
			else if (slaves.size() > nodelist.size())
			{
				cout << "[master]Number of slave connection exceeded allowed limits" << endl;
				cout << "[master]\tDebugging needed on this problem" << endl;
			}

			// sleeps for 0.0001 seconds. change this if necessary
			// usleep(100);
		}
	}

	struct sockaddr_un serveraddr;

	// SOCK_STREAM -> tcp
	ipcfd = socket (AF_UNIX, SOCK_STREAM, 0);

	if (ipcfd < 0)
	{
		cout << "[master]Openning unix socket error" << endl;
		exit (1);
	}

	memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
	serveraddr.sun_family = AF_UNIX;
	strcpy (serveraddr.sun_path, IPC_PATH);

	while (connect (ipcfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0)
	{
		usleep (1000);
	}

	// set socket to be nonblocking
	fcntl (ipcfd, F_SETFL, O_NONBLOCK);
	setsockopt (ipcfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
	setsockopt (ipcfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
	// dhtclient = new DHTclient(RAVENLEADER, dhtport);
	// set sockets to be non-blocking socket to avoid deadlock
	fcntl (serverfd, F_SETFL, O_NONBLOCK);

	for (int i = 0; (unsigned) i < slaves.size(); i++)
	{
		fcntl (slaves[i]->getfd(), F_SETFL, O_NONBLOCK);
	}

	// create listener thread and run
	thread_continue = true;
	pthread_t listener_thread;
	pthread_create (&listener_thread, NULL, signal_listener, (void*) &serverfd);


	// sleeping loop which prevents process termination
	while (thread_continue)
	{
		sleep (1);
	}

	//dhtserver->close();
	return 0;
}

void open_server (int port)
{
	struct sockaddr_in serveraddr;
	// socket open
	serverfd = socket (AF_INET, SOCK_STREAM, 0);

	if (serverfd < 0)
	{
		cout << "[master]Socket opening failed" << endl;
	}

	// bind
	int valid = 1;
	memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
	setsockopt (serverfd, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof (valid));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);
	serveraddr.sin_port = htons ( (unsigned short) port);

	if (bind (serverfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0)
	{
		cout << "[master]\033[0;31mBinding failed\033[0m" << endl;
		exit (1);
	}

	// listen
	if (listen (serverfd, BACKLOG) < 0)
	{
		cout << "[master]Listening failed" << endl;
		exit (1);
	}
}

void* signal_listener (void* args)
{
	int serverfd = * ( (int*) args);
	int readbytes = 0;
	int tmpfd = -1; // store fd of new connected node temporarily
	char* haddrp;
	struct sockaddr_in connaddr;
	int addrlen = sizeof (connaddr);
	int elapsed = 0; // time elapsed im msec
	struct timeval time_start;
	struct timeval time_end;
	gettimeofday (&time_start, NULL);
	gettimeofday (&time_end, NULL);

	//struct timeval job_start; // record when first job submitted
	//struct timeval map_end; // record every map phase end
	//struct timeval job_end; // record every job end

	//int scheduledjob = 0;

// MH :: Histogram
int total_serv = slaves.size();
int *hist      = new int[total_serv];

for ( int i = 0; i < total_serv; i++ ) {
	hist[i] = 0;
}

vector<vector<list<master_task*>>> local_tasks;
vector<list<master_task*>*> task_queue(total_serv);
vector<list<master_task*>*> next_task_queue(total_serv);
vector<list<master_task*>::iterator> task_queue_it(total_serv);
vector<unordered_map<string, bool>*> cmap(total_serv);
vector<unordered_map<string, bool>*> next_cmap(total_serv);
vector<int> num_replacable(total_serv);
vector<bool> next_queue_set(total_serv, false);
// vector<bool> latency_aware(total_serv, true);
vector<int> latency_aware(total_serv, 0);
const int num_la = 0;
vector<list<string>> file_history(total_serv);
// vector<unordered_map<string, bool>> h_map(total_serv);
vector<unordered_map<string, list<string>::iterator>> h_map(total_serv);

bool all_set = false;
for (int i = 0; i < total_serv; ++i) {
  task_queue[i] = new list<master_task*>();
  task_queue_it[i] = task_queue[i]->begin();
  next_task_queue[i] = new list<master_task*>();
  cmap[i] = new unordered_map<string, bool>();
  next_cmap[i] = new unordered_map<string, bool>();
}

	// listen signals from nodes and listen to node connection
	while (1)
	{
		// check client (or job) connection
		tmpfd = accept (serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);

		if (tmpfd >= 0)
		{
			// send "whoareyou" message to connected node
			memset (write_buf, 0, BUF_SIZE);
			strcpy (write_buf, "whoareyou");
			nbwrite (tmpfd, write_buf);
			memset (read_buf, 0, BUF_SIZE);
			// blocking read to check identification of connected node
			readbytes = nbread (tmpfd, read_buf);

			if (readbytes == 0)
			{
				cout << "[master]Connection closed from client before getting request" << endl;
				close (tmpfd);
				tmpfd = -1;
				continue;
			}

			fcntl (tmpfd, F_SETFL, O_NONBLOCK);   // set socket to be non-blocking socket to avoid deadlock

			if (strncmp (read_buf, "client", 6) == 0)       // if connected node is a client
			{
				// get ip address of client
				haddrp = inet_ntoa (connaddr.sin_addr);
				printf ("[master]Client node connected from %s \n", haddrp);
				clients.push_back (new connclient (tmpfd));
			}
			else if (strncmp (read_buf, "slave", 5) == 0)
			{
				cout << "[master]Unexpected connection from slave: " << endl;
				cout << "[master]Closing connection to the slave..." << endl;

				// check this code
				memset (write_buf, 0, BUF_SIZE);
				strcpy (write_buf, "close");
				nbwrite (tmpfd, write_buf);
				close (tmpfd);
			}
			else if (strncmp (read_buf, "job", 3) == 0)         // if connected node is a job
			{
// !!!_3
				// limit the maximum available job connection
				if (jobs.size() == (unsigned) max_job)
				{
					cout << "[master]Cannot accept any more job request due to maximum job connection limit" << endl;
					cout << "[master]\tClosing connection from the job..." << endl;
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "nospace");
					nbwrite (tmpfd, write_buf);
					close (tmpfd);
					break;
				}
        all_set = false;

				// send the job id to the job
				stringstream jobidss;
				jobidss << "jobid ";
				jobidss << jobidclock;
				memset (write_buf, 0, BUF_SIZE);
				strcpy (write_buf, jobidss.str().c_str());
				nbwrite (tmpfd, write_buf);
				cout << "[master]Job " << jobidclock << " arrived" << endl;
// !!!_4 Add master_job object with jobid.
				jobs.push_back (new master_job (jobidclock, tmpfd));
        jobs.back()->start_time_ = (int)std::time(NULL);
        local_tasks.emplace_back(vector<list<master_task*>>(total_serv));
				jobidclock++;

			}
			else
			{
				cout << "[master]Unidentified connected node: " << endl;
				cout << "[master]Closing connection to the node..." << endl;
				close (tmpfd);
			}
		}

		// listen to slaves
		for (int i = 0; (unsigned) i < slaves.size(); i++)
		{
			readbytes = nbread (slaves[i]->getfd(), read_buf);

			if (readbytes == 0)     // connection closed from slave
			{
				cout << "[master]Connection from a slave closed" << endl;
				delete slaves[i];
				slaves.erase (slaves.begin() + i);
				i--;
				continue;
			}
			else if (readbytes < 0)
			{
				continue;
			}
			else     // signal from the slave
			{
				// remove below "key" part
				// remove below "key" part
				// remove below "key" part
				/*
				   if(strncmp(read_buf, "key", 3) == 0) // key signal arrived
				   {
				   char* token;
				   master_job* thejob;
				   token = strtok(read_buf, " "); // token <- "key"
				   token = strtok(NULL, "\n"); // token <- job id

				   thejob = find_jobfromid(atoi(token));

				   token = strtok(NULL, "\n"); // token <- key

				   while(token != NULL)
				   {
				   thejob->add_key(token); // token
				   token = strtok(NULL, "\n");
				   }
				   }
				   */
				if (strncmp (read_buf, "peerids", 7) == 0) // from slaves
				{
					char* token;
					master_job* thejob;
					token = strtok (read_buf, " ");   // token <- "peerids"
					token = strtok (NULL, " ");   // token <- jobid
					thejob = find_jobfromid (atoi (token));
					// token first ids
					token = strtok (NULL, " ");

					while (token != NULL)
					{
						thejob->peerids.insert (atoi (token));
						token = strtok (NULL, " ");   // good
					}
				}
				else if (strncmp (read_buf, "taskcomplete", 12) == 0)         // "taskcomplete" signal arrived
				{
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
          
#ifdef ACS
          /*
          if (thetask->get_taskrole() == MAP) {
            string input_path = thetask->get_inputpath(0);
            int cidx = thetask->dht_loc;
            if (h_map[cidx].find(input_path) == h_map[cidx].end()) {
              file_history[cidx].emplace_back(input_path);
              auto h_it = file_history[cidx].end();
              --h_it;
              h_map[cidx].emplace(input_path, h_it);
              if (h_map[cidx].size() > 40) {
                h_map[cidx].erase(file_history[cidx].front());
                file_history[cidx].pop_front();
              }
            } else {
              file_history[cidx].erase(h_map[cidx][input_path]);
              file_history[cidx].emplace_back(input_path);
              auto h_it = file_history[cidx].end();
              --h_it;
              h_map[cidx][input_path] = h_it;
            }
          }
          */
#endif


					cout << "[master]A task completed(jobid: " << ajobid << ", " << thejob->get_numcompleted_tasks();
					cout << "/" << thejob->get_numtasks() << ")" << endl;
				}
				else
				{
					cout << "[master]Undefined message from slave node: " << read_buf << endl;
					cout << "[master]Undefined message size:" << readbytes << endl;
				}
			}
		}

		// listen to clients
		for (int i = 0; (unsigned) i < clients.size(); i++)
		{
			readbytes = nbread (clients[i]->getfd(), read_buf);

			if (readbytes == 0)     // connection closed from client
			{
				cout << "[master]Connection from a client closed" << endl;
				delete clients[i];
				clients.erase (clients.begin() + i);
				i--;
				continue;
			}
			else if (readbytes < 0)
			{
				continue;
			}
			else     // signal from the client
			{
				cout << "[master]Message accepted from client: " << read_buf << endl;

				if (strncmp (read_buf, "stop", 4) == 0)       // "stop" signal arrived
				{
					int currfd = clients[i]->getfd(); // store the current client's fd

					// stop all slave
					for (int j = 0; (unsigned) j < slaves.size(); j++)
					{
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, "close");
						nbwrite (slaves[j]->getfd(), write_buf);
						// blocking read from slave
						fcntl (slaves[j]->getfd(), F_SETFL, fcntl (slaves[j]->getfd(), F_GETFL) & ~O_NONBLOCK);
						readbytes = nbread (slaves[j]->getfd(), read_buf);

						if (readbytes == 0)     // closing slave succeeded
						{
							delete slaves[j];
							slaves.erase (slaves.begin() + j);
							j--;
						}
						else     // message arrived before closed
						{
							memset (write_buf, 0, BUF_SIZE);
							strcpy (write_buf, "ignored");
							nbwrite (slaves[j]->getfd(), write_buf);
							j--;
							continue;
						}

						cout << "[master]Connection from a slave closed" << endl;
					}

					cout << "[master]All slaves closed" << endl;

					// stop all client except the one requested stop
					for (int j = 0; (unsigned) j < clients.size(); j++)
					{
						if (currfd == clients[j]->getfd())   // except the client who requested the stop
						{
							continue;
						}

						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, "close");
						nbwrite (clients[j]->getfd(), write_buf);
						// blocking read from client
						fcntl (clients[j]->getfd(), F_SETFL, fcntl (clients[j]->getfd(), F_GETFL) & ~O_NONBLOCK);
						readbytes = nbread (clients[j]->getfd(), read_buf);

						if (readbytes == 0)     // closing client succeeded
						{
							delete clients[j];
							clients.erase (clients.begin() + j);
							j--;
						}
						else     // message arrived before closed
						{
							memset (write_buf, 0, BUF_SIZE);
							strcpy (write_buf, "ignored");
							nbwrite (clients[j]->getfd(), write_buf);
							j--;
							continue;
						}

						cout << "[master]Connection from a client closed" << endl;
					}

					cout << "[master]All clients closed" << endl;
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "result: stopping successful");
					nbwrite (clients[i]->getfd(), write_buf);
				}
				else if (strncmp (read_buf, "numslave", 8) == 0)         // "numslave" signal arrived
				{
					string ostring = "result: number of slave nodes = ";
					stringstream ss;
					ss << slaves.size();
					ostring.append (ss.str());
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, ostring.c_str());
					nbwrite (clients[i]->getfd(), write_buf);
				}
				else if (strncmp (read_buf, "numclient", 9) == 0)         // "numclient" signal arrived
				{
					string ostring = "result: number of client nodes = ";
					stringstream ss;
					ss << clients.size();
					ostring.append (ss.str());
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, ostring.c_str());
					nbwrite (clients[i]->getfd(), write_buf);
				}
				else if (strncmp (read_buf, "numjob", 6) == 0)         // "numjob" signal arrived
				{
					string ostring = "result: number of running jobs = ";
					stringstream ss;
					ss << jobs.size();
					ostring.append (ss.str());
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, ostring.c_str());
					nbwrite (clients[i]->getfd(), write_buf);
				}
				else if (strncmp (read_buf, "numtask", 7) == 0)         // "numtask" signal arrived
				{
					string ostring = "result: number of running tasks = ";
					stringstream ss;
					int numtasks = 0;

					for (int j = 0; (unsigned) j < jobs.size(); j++)
					{
						numtasks += jobs[j]->get_numrunning_tasks();
					}

					ss << numtasks;
					ostring.append (ss.str());
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, ostring.c_str());
					nbwrite (clients[i]->getfd(), write_buf);
				}
				else     // undefined signal
				{
					cout << "[master]Undefined signal from client: " << read_buf << endl;
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "result: error. the request is unknown");
					nbwrite (clients[i]->getfd(), write_buf);
				}
			}
		}

		// check messages from jobs
		for (int i = 0; (unsigned) i < jobs.size(); i++)
		{
			readbytes = nbread (jobs[i]->getjobfd(), read_buf);

			if (readbytes == 0)     // connection to the job closed. maybe process terminated
			{
				delete jobs[i];
				jobs.erase (jobs.begin() + i);
				i--;
				cout << "[master]Job terminated abnormally" << endl;
				continue;
			}
			else if (readbytes < 0)
			{
				// do nothing
			}
			else     // signal from the job
			{
				if (strncmp (read_buf, "complete", 8) == 0)       // "succompletion" signal arrived
				{
					cout << "[master]Job " << jobs[i]->getjobid() << " successfully completed" << endl;
					// clear up the completed job
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "terminate");
					nbwrite (jobs[i]->getjobfd(), write_buf);
					// delete the job from the vector jobs
					delete jobs[i];
					jobs.erase (jobs.begin() + i);
          local_tasks.erase (local_tasks.begin() + i);

					i--;
				}
				else if (strncmp (read_buf, "jobconf", 7) == 0)         // "jobconf" message arrived
				{
					char* token;
					token = strtok (read_buf, " ");   // token -> jobconf
					token = strtok (NULL, " ");   // token -> number of inputpaths

					// parse all configure
					while (token != NULL)
					{
						if (strncmp (token, "argcount", 8) == 0)
						{
							// NOTE: there should be at least 1 arguments(program path name)
							token = strtok (NULL, " ");   // token <- argument count
							jobs[i]->setargcount (atoi (token));
							// process next configure
							token = strtok (NULL, " ");   // token -> argvalues

							// check the protocol
							if (strncmp (token, "argvalues", 9) != 0)     // if the token is not 'argvalues'
							{
								cout << "Debugging: the 'jobconf' protocol conflicts." << endl;
							}

							char** arguments = new char*[jobs[i]->getargcount()];

							for (int j = 0; j < jobs[i]->getargcount(); j++)
							{
								token = strtok (NULL, " ");
								arguments[j] = new char[strlen (token) + 1];
								strcpy (arguments[j], token);
							}

							jobs[i]->setargvalues (arguments);
						}
						/*
						   else if(strncmp(token, "inputpath", 9) == 0)
						   {
						   int numpath;
						   string tmp;
						   token = strtok(NULL, " "); // token -> number of input paths

						   numpath = atoi(token);
						   for(int j=0;j<numpath;j++)
						   {
						   tmp = strtok(NULL, " ");
						   jobs[i]->add_inputpath(tmp);
						   }
						   }
						   */
						else if (strncmp (token, "nummap", 6) == 0)
						{
							int nummap;
							token = strtok (NULL, " ");   // token <- number of maps
							nummap = atoi (token);
							jobs[i]->setnummap (nummap);
						}
						else if (strncmp (token, "numreduce", 9) == 0)
						{
// !!!_8 Receive "jobconf nummap numreduce" message from app.
// Wait for the "inputpath" message.
							int numreduce;
							token = strtok (NULL, " ");   // token <- number of reduces
							numreduce = atoi (token);
							jobs[i]->setnumreduce (numreduce);
							// numreduce is the last token of this message
							// read another message, which is inputpath message
							int numpath;
							int iter = 0;
							readbytes = -1;

							while (readbytes < 0)
							{
								readbytes = nbread (jobs[i]->getjobfd(), read_buf);
							}
// !!!_10 "inputpath" message from app arrived.
// Receive inputpaths from app and add it to the master_job object.

							token = strtok (read_buf, " ");   // token <- "inputpath"
							token = strtok (NULL, " ");   // token <- number of input paths
							numpath = atoi (token);

							while (iter < numpath)
							{
								token = strtok (NULL, " ");   // next input path or NULL pointer

								if (token == NULL)     // a null pointer
								{
									readbytes = -1;

									while (readbytes < 0)
									{
										readbytes = nbread (jobs[i]->getjobfd(), read_buf);
									}

									token = strtok (read_buf, " ");   // must be a valid token(input path)
									jobs[i]->add_inputpath (token);
								}
								else     // a valid input path
								{
									jobs[i]->add_inputpath (token);
								}

								iter++;
							}
						}
						else
						{
							cout << "[master]Unknown job configure message from job: " << token << endl;
						}

						// process next configure
						token = strtok (NULL, " ");
					}

					if (jobs[i]->getnummap() == 0)
					{
						jobs[i]->setnummap (jobs[i]->get_numinputpaths());
					}

// !!!_11 Create map tasks and allocate inputpaths to created map tasks.
// All the added tasks will be added to waiting_tasks queue.
// (Default number of map task is same as the number of inputpaths.)
					// create map tasks
					for (int j = 0; j < jobs[i]->getnummap(); j++)
					{
						jobs[i]->add_task (new master_task (jobs[i], MAP));
					}

					// map inputpaths to each map tasks
					int path_iteration = 0;
          vector<list<string>> local_inputs(slaves.size());

					while (path_iteration < jobs[i]->get_numinputpaths()) {
						for (int j = 0; j < jobs[i]->get_numtasks() && path_iteration < jobs[i]->get_numinputpaths(); j++)
						{
              auto atask = jobs[i]->get_task(j);
              string thepath = jobs[i]->get_inputpath(path_iteration);
              memset(write_buf, 0, HASHLENGTH);
              strcpy(write_buf, thepath.c_str());
              uint32_t hash_value = h(write_buf, HASHLENGTH);
							atask->add_inputpath(thepath);
              atask->dht_loc = hash_value % slaves.size();
              local_inputs[atask->dht_loc].push_back(thepath);
							path_iteration++;
						}
					}

          for (int j = 0; j < slaves.size(); ++j) {
            string message = "caching";
            message.append(" ");
            message.append(to_string(jobs[i]->getjobid()));
            memset(write_buf, 0, BUF_SIZE);
            strcpy(write_buf, message.c_str());
            nbwrite(slaves[j]->getfd(), write_buf);
            auto iter = local_inputs[j].begin();
            message = "cinputpath";
            while (iter != local_inputs[j].end()) {
              if (message.length() + (*iter).length() + 1 < BUF_SIZE) {
                message.append (" ");
                message.append (*iter);
              } else {
                if ((*iter).length() + 11 > BUF_SIZE) {
                  cout << "[master]The length of cinputpath exceeded the limit" << endl;
                }
                // send message to slave
                memset (write_buf, 0, BUF_SIZE);
                strcpy (write_buf, message.c_str());
                nbwrite (slaves[j]->getfd(), write_buf);
                message = "cinputpath ";
                message.append (*iter);
              }
              ++iter;
            }
            // send remaining paths
            if (message.length() > strlen ("cinputpath ")) {
              memset (write_buf, 0, BUF_SIZE);
              strcpy (write_buf, message.c_str());
              nbwrite (slaves[j]->getfd(), write_buf);
            }
            // notify end of cinputpaths
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf, "Ecinput");
            nbwrite (slaves[j]->getfd(), write_buf);
          }

					// set job stage as MAP_STAGE
					jobs[i]->set_stage (MAP_STAGE);
				}
				else     // undefined signal
				{
					cout << "[master]Undefined signal from job: " << read_buf << endl;
				}
			}

			// check if all task finished
			if (jobs[i]->get_numtasks() == jobs[i]->get_numcompleted_tasks())
			{
				if (jobs[i]->get_stage() == MAP_STAGE)     // if map stage is finished
				{
					if (jobs[i]->status == TASK_FINISHED)     // information of numblock is gathered
					{
            for (int j = 0; j < slaves.size(); ++j) {
              string message = "Ecaching";
              message.append(" ");
              message.append(to_string(jobs[i]->getjobid()));
              memset(write_buf, 0, BUF_SIZE);
              strcpy(write_buf, message.c_str());
              nbwrite(slaves[j]->getfd(), write_buf);
            }
						// send message to the job to inform that map phase is completed


						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, "mapcomplete");
						nbwrite (jobs[i]->getjobfd(), write_buf);
						// request to the cache the flush of each iwriter and information of numblock from each peer
						stringstream ss;
						string message;
						ss << "iwritefinish ";
						ss << jobs[i]->getjobid();

						// append peer ids to ss
						for (set<int>::iterator it = jobs[i]->peerids.begin(); it != jobs[i]->peerids.end(); it++)
						{
							ss << " ";
							ss << *it;
						}

						message = ss.str();
						// send the message to the cache server
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, message.c_str());
						nbwrite (ipcfd, write_buf);
						jobs[i]->status = REQUEST_SENT;
					}
					else if (jobs[i]->status == REQUEST_SENT)
					{
						// do nothing(just wait for the respond)
					}
					else     // status == RESPOND_RECEIVED
					{
						// jobs[i]->hist_not_processed();
						// determine the number of reducers
						if (jobs[i]->getnumreduce() <= 0)
						{
							//jobs[i]->setnumreduce (jobs[i]->peerids.size());
							jobs[i]->setnumreduce (jobs[i]->peerids.size() * REDUCE_SLOT);
//cout << "[wb] numreduce was <=0, so it is now set to " << jobs[i]->peerids.size() * REDUCE_SLOT << endl;
						}
						/*
						else if ( (unsigned) jobs[i]->getnumreduce() > jobs[i]->peerids.size())
						{
							// TODO: enable much more number of reducers
							jobs[i]->setnumreduce (jobs[i]->peerids.size());
//cout << "[wb] numreduce is too large. so it is now set to " << jobs[i]->peerids.size() * REDUCE_SLOT << endl;
						}
						*/

						// generate reduce tasks and feed each reducer dedicated peer with numblock information
						for (int j = 0; j < jobs[i]->getnumreduce(); j++)
						{
							master_task* newtask = new master_task (jobs[i], REDUCE);
							jobs[i]->add_task (newtask);
						}

						int index = 0;
						int iteration = 0;

//for (int j = 0; j < jobs[i]->numiblocks.size(); ++j)
	//printf("jobs[%d]->numiblocks[%d] = %d\n", i, j, jobs[i]->numiblocks[j]);

						// while all peerids are given to reducers
//printf("[master] peerids.size() = %d\n", peerids.size());
						/*
						for (set<int>::iterator it = jobs[i]->peerids.begin(); it != jobs[i]->peerids.end(); it++)
						{
							int* tmparray = new int[REDUCE_SLOT];
							for (int j = 0; j < REDUCE_SLOT; ++j) {
								tmparray[j] = jobs[i]->numiblocks[index];
								index++;
							}
							jobs[i]->get_waitingtask (iteration)->numiblocks.push_back (tmparray);
							jobs[i]->get_waitingtask (iteration)->peerids.push_back (*it);
							//index++;
							iteration++;

							if (iteration >= jobs[i]->getnumreduce())
							{
								iteration = 0;
							}
						}
						*/

						for (auto it = jobs[i]->peerids.begin(); it != jobs[i]->peerids.end(); ++it) {
							for (int j = 0; j < REDUCE_SLOT; ++j) {
								int tmp_numiblock = jobs[i]->numiblocks[index++];
								master_task *tmp_task = jobs[i]->get_waitingtask(iteration++);

								tmp_task->peerids.push_back(*it);
								tmp_task->numiblocks.push_back(tmp_numiblock);
								tmp_task->threadids.push_back(j);
                tmp_task->dht_loc = tmp_task->peerids[0];

								if (iteration >= jobs[i]->getnumreduce())
									iteration = 0;
							}
						}

						jobs[i]->set_stage (REDUCE_STAGE);
					}
				}
				else if (jobs[i]->get_stage() == REDUCE_STAGE)       // if reduce stage is finished
				{
					// send message to the job to complete the job
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "complete");
					nbwrite (jobs[i]->getjobfd(), write_buf);
					cout << "[master]Job " << jobs[i]->getjobid() << " completed successfully" << endl;
					jobs[i]->set_stage (COMPLETED_STAGE);
					// clear the job from the vector and finish
					delete jobs[i];
					jobs.erase (jobs.begin() + i);
					i--;
					continue;
				}
				else
				{
					// pass the case for INITIAL_STAGE and COMPLETED_STAGE
				}
			}
		}

		// receive message from cache server
		readbytes = nbread (ipcfd, read_buf);

		if (readbytes > 0)
		{
			if (strncmp (read_buf, "numblocks", 9) == 0)
			{
printf("[master] from cacheserver: %s\n", read_buf);
				char* token;
				int jobid;
				//int iwf_size;
				token = strtok (read_buf, " ");   // token: "numblocks"
				token = strtok (NULL, " ");   // token: jobid
				jobid = atoi (token);
				token = strtok (NULL, " ");
				//iwf_size = atoi(token);
				token = strtok (NULL, " ");   // first number of block

				//for (int i = 0; jobs.size(); i++) {
				for (int i = 0; (unsigned)i < jobs.size(); i++) {
					if (jobs[i]->getjobid() == jobid)
					{
						while (token != NULL)
						{
							jobs[i]->numiblocks.push_back (atoi (token));
							token = strtok (NULL, " ");
						}

						jobs[i]->status = RESPOND_RECEIVED;
						break;
					}
				}
			}
		}
		else if (readbytes == 0)
		{
			cout << "[master]Connection to cache server abnormally closed" << endl;
			usleep (100000);
		}

#ifdef NEW_SCHED
for ( int i = 0; (unsigned) i < jobs.size(); i++ ) {
	if ( jobs[i]->is_histed() == false && (jobs[i]->getnummap() > 0 || jobs[i]->getnumreduce() > 0) ) {
    int* job_hist = jobs[i]->create_hist(total_serv);
		while ( jobs[i]->get_numwaiting_tasks() > 0 ) {
			master_task* thetask = jobs[i]->pop_lastwaitingtask();
			if ( thetask == NULL ) continue;
			if ( thetask->get_taskrole() == MAP ) {
				string thepath = thetask->get_inputpath(0);
				memset (write_buf, 0, HASHLENGTH);
				strcpy (write_buf, thepath.c_str());
				uint32_t hash_value = h ( write_buf, HASHLENGTH );
				int key = thehistogram->get_index (hash_value);
				jobs[i]->hist_insert ({key+1000*job_hist[key], thetask});
				++hist[key];
				++job_hist[key];
			} else {
				int key = thetask->peerids[0];
				jobs[i]->hist_insert ({key+1000*job_hist[key], thetask});
				++hist[key];
				++job_hist[key];
			}
		}
		jobs[i]->hist_processed();
		cout << "[master] [debug] histogram is created" << endl;
		for ( int j = 0; (unsigned) j < slaves.size(); j++ ) {
			cout << j << " " << hist[j] << endl;
		}
	}
}

if ( jobs.size() > 1 ) std::sort(jobs.begin(), jobs.end(), jobComp);
for ( int i = 0; (unsigned) i < slaves.size(); i ++ ) {
	if ( hist[i] > 0 ) {
//		int key = i + 1000 * (hist[i]-1);
//		cout << i << " " << hist[i] << " " << key << endl;
		for ( int j = 0; (unsigned) j < jobs.size(); j++ ) {
      int * job_hist = jobs[j]->get_hist();
			if ( job_hist == NULL || job_hist[i] == 0 ) continue;
      int key = i + 1000 * (job_hist[i]-1);
			master_task* thetask = jobs[j]->hist_get(key);
			if ( thetask == NULL ) continue;		
			if ( thetask->get_taskrole() == MAP ) {
				if ( slaves[i]->getnumrunningtasks() < slaves[i]->getmaxmaptask() ) {
					stringstream ss;
					ss << "tasksubmit ";
					ss << jobs[j]->getjobid();
					ss << " ";
					ss << thetask->gettaskid();
					ss << " ";
					ss << "MAP";
					ss << " ";
					ss << thetask->get_job()->getargcount();

					for (int k = 0; k < thetask->get_job()->getargcount(); k++)
					{
						ss << " ";
						ss << thetask->get_job()->getargvalue (k);
					}
					string message = ss.str();
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (slaves[i]->getfd(), write_buf);
					// prepare inputpath message
					int iter = 0;
					message = "inputpath";

					while (iter < thetask->get_numinputpaths())
					{
						//							cout << jobs[j]->getjobid() << endl;
						//							cout << thetask->get_inputpath(iter) << endl;
						if (message.length() + thetask->get_inputpath (iter).length() + 1 < BUF_SIZE)
						{
							message.append (" ");
							message.append (thetask->get_inputpath (iter));
						}
						else
						{
							if (thetask->get_inputpath (iter).length() + 10 > BUF_SIZE)
							{
								cout << "[master]The length of inputpath exceeded the limit" << endl;
							}

							// send message to slave
							memset (write_buf, 0, BUF_SIZE);
							strcpy (write_buf, message.c_str());
							nbwrite (slaves[i]->getfd(), write_buf);
							message = "inputpath ";
							message.append (thetask->get_inputpath (iter));
						}

						iter++;
					}

					// send remaining paths
					if (message.length() > strlen ("inputpath "))
					{
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, message.c_str());
						nbwrite (slaves[i]->getfd(), write_buf);
					}

					// notify end of inputpaths
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "Einput");
					nbwrite (slaves[i]->getfd(), write_buf);
					// forward waiting task to slave slot
					jobs[j]->schedule_task (key, slaves[i]);
					--job_hist[i];
					--hist[i];
					// sleeps for 0.0001 seconds. change this if necessary
					// usleep(100000);
					break;
				} else {
					continue;
				}
			} else {
				if ( slaves[i]->getnumrunningtasks() < slaves[i]->getmaxreducetask() ) {
					stringstream ss;
					ss << "tasksubmit ";
					ss << jobs[j]->getjobid();
					ss << " ";
					ss << thetask->gettaskid();
					ss << " ";
					ss << "REDUCE";
					ss << " ";
					ss << thetask->get_job()->getargcount();

					for (int k = 0; k < thetask->get_job()->getargcount(); k++)
					{
						ss << " ";
						ss << thetask->get_job()->getargvalue (k);
					}
					string message = ss.str();
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (slaves[i]->getfd(), write_buf);
					// prepare inputpath message
					message = SetInputPathMessage(thetask);

					// notify end of inputpaths
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (slaves[i]->getfd(), write_buf);
					// forward waiting task to slave slot
					jobs[j]->schedule_task (key, slaves[i]);
					--job_hist[i];
					--hist[i];
					// sleeps for 0.0001 seconds. change this if necessary
					// usleep(100000);
					break;
				}
			}
		}
	} else {
//		if ( hist_max > 0 ) cout << i << " " << hist[i] << " " << key << endl;
		for ( int j = 0; (unsigned) j < jobs.size(); j++ ) {
      int * job_hist = jobs[j]->get_hist();
      if ( job_hist == NULL ) continue;
      int hist_max = 0;
      int hist_max_idx = 0;
      for ( int j = 0 ; j < total_serv; j++ ) {
        if ( job_hist[j] > hist_max ) {
          hist_max_idx = j;
          hist_max = job_hist[j];
        }
      }
      int key = hist_max_idx + 1000 * (hist_max-1);
			master_task* thetask = jobs[j]->hist_get(key);
			if ( thetask == NULL ) continue;		
			if ( thetask->get_taskrole() == MAP ) {
				if ( slaves[i]->getnumrunningtasks() < slaves[i]->getmaxmaptask() ) {
					stringstream ss;
					ss << "tasksubmit ";
					ss << jobs[j]->getjobid();
					ss << " ";
					ss << thetask->gettaskid();
					ss << " ";
					ss << "MAP";
					ss << " ";
					ss << thetask->get_job()->getargcount();

					for (int k = 0; k < thetask->get_job()->getargcount(); k++)
					{
						ss << " ";
						ss << thetask->get_job()->getargvalue (k);
					}
					string message = ss.str();
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (slaves[i]->getfd(), write_buf);
					// prepare inputpath message
					int iter = 0;
					message = "inputpath";

					while (iter < thetask->get_numinputpaths())
					{
						//							cout << jobs[j]->getjobid() << endl;
						//							cout << thetask->get_inputpath(iter) << endl;
						if (message.length() + thetask->get_inputpath (iter).length() + 1 < BUF_SIZE)
						{
							message.append (" ");
							message.append (thetask->get_inputpath (iter));
						}
						else
						{
							if (thetask->get_inputpath (iter).length() + 10 > BUF_SIZE)
							{
								cout << "[master]The length of inputpath exceeded the limit" << endl;
							}

							// send message to slave
							memset (write_buf, 0, BUF_SIZE);
							strcpy (write_buf, message.c_str());
							nbwrite (slaves[i]->getfd(), write_buf);
							message = "inputpath ";
							message.append (thetask->get_inputpath (iter));
						}

						iter++;
					}

					// send remaining paths
					if (message.length() > strlen ("inputpath "))
					{
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, message.c_str());
						nbwrite (slaves[i]->getfd(), write_buf);
					}

					// notify end of inputpaths
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "Einput");
					nbwrite (slaves[i]->getfd(), write_buf);
					// forward waiting task to slave slot
					jobs[j]->schedule_task (key, slaves[i]);
					--hist[hist_max_idx];
					--job_hist[hist_max_idx];
					// sleeps for 0.0001 seconds. change this if necessary
					// usleep(100000);
					break;
				} else {
					continue;
				}
			} else {
        continue;
//				if ( slaves[i]->getnumrunningtasks() < slaves[i]->getmaxreducetask() ) {
//					stringstream ss;
//					ss << "tasksubmit ";
//					ss << jobs[j]->getjobid();
//					ss << " ";
//					ss << thetask->gettaskid();
//					ss << " ";
//					ss << "REDUCE";
//					ss << " ";
//					ss << thetask->get_job()->getargcount();
//
//					for (int k = 0; k < thetask->get_job()->getargcount(); k++)
//					{
//						ss << " ";
//						ss << thetask->get_job()->getargvalue (k);
//					}
//					string message = ss.str();
//					memset (write_buf, 0, BUF_SIZE);
//					strcpy (write_buf, message.c_str());
//					nbwrite (slaves[i]->getfd(), write_buf);
//					// prepare inputpath message
//					message = SetInputPathMessage(thetask);
//
//					// notify end of inputpaths
//					memset (write_buf, 0, BUF_SIZE);
//					strcpy (write_buf, message.c_str());
//					nbwrite (slaves[i]->getfd(), write_buf);
//					// forward waiting task to slave slot
//					jobs[j]->schedule_task (key, slaves[i]);
//					--hist[hist_max_idx];
//					// sleeps for 0.0001 seconds. change this if necessary
//					// usleep(100000);
//					break;
//				}
			}
		}
	}
}

#endif
#ifdef DELAY
		int delayThreashold = 5;

		for ( int i = 0; (unsigned) i < slaves.size(); i++ ) {
      if (slaves[i]->getnumrunningtasks() >= slaves[i]->getmaxmaptask())
        continue;
			std::sort(jobs.begin(), jobs.end(), jobComp);
			for ( int j = 0; (unsigned) j < jobs.size(); j++ ) {
        master_task *thetask = NULL;
        for (int jj = 0; jj < jobs[j]->get_numwaiting_tasks(); ++jj) {
          master_task *atask = jobs[j]->get_waitingtask(jj);
          int nodeidx = -1;
          if (atask == NULL) continue; 
          if (atask->get_taskrole() == MAP) {
            string thepath = atask->get_inputpath (0);
            memset (write_buf, 0, HASHLENGTH);
            strcpy (write_buf, thepath.c_str());
            uint32_t hash_value = h ( write_buf, HASHLENGTH );
            nodeidx = hash_value % slaves.size();
          } else {
            nodeidx = atask->peerids[0];
          }
          if (nodeidx == i) {
            thetask = atask;
            break;
          } else if ((jobs[j]->getdelayed() > delayThreashold)
              && (thetask == NULL)) {
            thetask = atask;
          }
        }
        if (thetask != NULL) {
          if (thetask->get_taskrole() == MAP) {
            jobs[j]->sched();
            stringstream ss;
            ss << "tasksubmit ";
            ss << jobs[j]->getjobid();
            ss << " ";
            ss << thetask->gettaskid();
            ss << " ";
            ss << "MAP";
            ss << " ";
            ss << thetask->get_job()->getargcount();

            for (int k = 0; k < thetask->get_job()->getargcount(); k++)
            {
              ss << " ";
              ss << thetask->get_job()->getargvalue (k);
            }
            string message = ss.str();
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf, message.c_str());
            nbwrite (slaves[i]->getfd(), write_buf);
            // prepare inputpath message
            int iter = 0;
            message = "inputpath";

            while (iter < thetask->get_numinputpaths())
            {
//							cout << jobs[j]->getjobid() << endl;
//							cout << thetask->get_inputpath(iter) << endl;
              if (message.length() + thetask->get_inputpath (iter).length() + 1 < BUF_SIZE)
              {
                message.append (" ");
                message.append (thetask->get_inputpath (iter));
              }
              else
              {
                if (thetask->get_inputpath (iter).length() + 10 > BUF_SIZE)
                {
                  cout << "[master]The length of inputpath exceeded the limit" << endl;
                }

                // send message to slave
                memset (write_buf, 0, BUF_SIZE);
                strcpy (write_buf, message.c_str());
                nbwrite (slaves[i]->getfd(), write_buf);
                message = "inputpath ";
                message.append (thetask->get_inputpath (iter));
              }

              iter++;
            }

            // send remaining paths
            if (message.length() > strlen ("inputpath "))
            {
              memset (write_buf, 0, BUF_SIZE);
              strcpy (write_buf, message.c_str());
              nbwrite (slaves[i]->getfd(), write_buf);
            }

            // notify end of inputpaths
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf, "Einput");
            nbwrite (slaves[i]->getfd(), write_buf);
            // forward waiting task to slave slot
            jobs[j]->schedule_task (thetask, slaves[i]);
            // sleeps for 0.0001 seconds. change this if necessary
            // usleep(100000);
            break;
          } else {
            jobs[j]->sched();
            stringstream ss;
            ss << "tasksubmit ";
            ss << jobs[j]->getjobid();
            ss << " ";
            ss << thetask->gettaskid();
            ss << " ";
            ss << "REDUCE";
            ss << " ";
            ss << thetask->get_job()->getargcount();

            for (int k = 0; k < thetask->get_job()->getargcount(); k++)
            {
              ss << " ";
              ss << thetask->get_job()->getargvalue (k);
            }
            string message = ss.str();
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf, message.c_str());
            nbwrite (slaves[i]->getfd(), write_buf);
            // prepare inputpath message
            message = SetInputPathMessage(thetask);

            // notify end of inputpaths
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf, message.c_str());
            nbwrite (slaves[i]->getfd(), write_buf);
            // forward waiting task to slave slot
            jobs[j]->schedule_task (thetask, slaves[i]);
            // sleeps for 0.0001 seconds. change this if necessary
            // usleep(100000);
            break;
          }
        } else { 
          jobs[j]->delayed((int)std::time(NULL));
        }// thetask == NULL
      }  // j: jobs
		}  // i: slaves


		// process and schedule jobs and tasks
#endif
#ifdef FCFS

		// default scheduler: FCFS-like scheduler
		for (int i = 0; (unsigned) i < jobs.size(); i++)
		{
			for (int j = 0; (unsigned) j < slaves.size(); j++)
			{
				while ( (slaves[j]->getnumrunningtasks() < slaves[j]->getmaxtask())
						&& (jobs[i]->get_lastwaitingtask() != NULL))      // schedule all the slot until none of the slot is avilable
				{
					master_task* thetask = jobs[i]->get_lastwaitingtask();
					// write to the slave the task information
					stringstream ss;
					ss << "tasksubmit ";
					ss << jobs[i]->getjobid();
					ss << " ";
					ss << thetask->gettaskid();
					ss << " ";

					if (thetask->get_taskrole() == MAP)
					{
						ss << "MAP";
					}
					else if (thetask->get_taskrole() == REDUCE)
					{
						ss << "REDUCE";
					}
					else
					{
						cout << "[master]Debugging: the role of the task not defined in the initialization step";
					}

					ss << " ";
					ss << thetask->get_job()->getargcount();
					// NOTE: there should be at leat 1 arguments(program path name)
					ss << " ";

					for (int k = 0; k < thetask->get_job()->getargcount(); k++)
					{
						ss << " ";
						ss << thetask->get_job()->getargvalue (k);
					}

					string message = ss.str();
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (slaves[j]->getfd(), write_buf);
					// prepare inputpath message
					int iter = 0;
					message = "inputpath";

					while (iter < thetask->get_numinputpaths())
					{
						if (message.length() + thetask->get_inputpath (iter).length() + 1 <= BUF_SIZE)
						{
							message.append (" ");
							message.append (thetask->get_inputpath (iter));
						}
						else
						{
							if (thetask->get_inputpath (iter).length() + 10 > BUF_SIZE)
							{
								cout << "[master]The length of inputpath exceeded the limit" << endl;
							}

							// send message to slave
							memset (write_buf, 0, BUF_SIZE);
							strcpy (write_buf, message.c_str());
							nbwrite (slaves[j]->getfd(), write_buf);
							message = "inputpath ";
							message.append (thetask->get_inputpath (iter));
						}

						iter++;
					}

					// send remaining paths
					if (message.length() > strlen ("inputpath "))
					{
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, message.c_str());
						nbwrite (slaves[j]->getfd(), write_buf);
					}

					// notify end of inputpaths
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "Einput");
					nbwrite (slaves[j]->getfd(), write_buf);
					// forward waiting task to slave slot
					jobs[i]->schedule_task (thetask, slaves[j]);
					// sleeps for 0.0001 seconds. change this if necessary
					// usleep(100000);
				}
			}
		}

#endif
#ifdef EMKDE

		scheduled = false;
		// EMKDE scheduler: schedule the task where the input is located
		for (int iter = 0; (unsigned) iter < jobs.size(); iter++)
		{
			int i = (scheduledjob + iter) % jobs.size();

			int nodeindex = -1;

			if (jobs[i]->get_lastwaitingtask() == NULL)
			{
				continue;
			}

			for (int k = 0; k < jobs[i]->get_numwaiting_tasks(); k++)
			{
				nodeindex = -1;
				master_task* thetask = jobs[i]->get_waitingtask (k);

				if (thetask->get_taskrole() == MAP)
				{
					string thepath = thetask->get_inputpath (0);   // first input as a representative input
					string address;
					stringstream tmpss;
					memset (write_buf, 0, HASHLENGTH);
					strcpy (write_buf, thepath.c_str());
// !!!_12 Calculate hash_value of the inputpath and pass the task
// to the slave that is related to the hash_value.
// "tasksubmit" message will be sent to the slave.
					// determine the hash value and count the query
					uint32_t hashvalue = h (write_buf, HASHLENGTH);
					nodeindex = thehistogram->get_index (hashvalue);

					if (slaves[nodeindex]->getnumrunningtasks() >= slaves[nodeindex]->getmaxmaptask()) //getmaxmaptask = MAP_SLOT
					{
						continue;
					}

					thehistogram->count_query (hashvalue);
					// write to the slave the task information
					stringstream ss;
					ss << "tasksubmit ";
					ss << jobs[i]->getjobid();
					ss << " ";
					ss << thetask->gettaskid();
					ss << " ";
					ss << "MAP";
					ss << " ";
					ss << thetask->get_job()->getargcount();

					for (int j = 0; j < thetask->get_job()->getargcount(); j++)
					{
						ss << " ";
						ss << thetask->get_job()->getargvalue (j);
					}

					string message = ss.str();
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (slaves[nodeindex]->getfd(), write_buf);
					// prepare inputpath message
					int it = 0;
					message = "inputpath";

					while (it < thetask->get_numinputpaths())
					{
						if (message.length() + thetask->get_inputpath (it).length() + 1 < BUF_SIZE)
						{
							message.append (" ");
							message.append (thetask->get_inputpath (it));
						}
						else
						{
							if (thetask->get_inputpath (it).length() + 10 > BUF_SIZE)
							{
								cout << "[master]The length of inputpath exceeded the limit" << endl;
							}

							// send message to slave
							memset (write_buf, 0, BUF_SIZE);
							strcpy (write_buf, message.c_str());
							nbwrite (slaves[nodeindex]->getfd(), write_buf);
							message = "inputpath ";
							message.append (thetask->get_inputpath (it));
						}

						it++;
					}

					// send remaining paths
					if (message.length() > strlen ("inputpath "))
					{
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, message.c_str());
						nbwrite (slaves[nodeindex]->getfd(), write_buf);
					}

					// notify end of inputpaths
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "Einput");
					nbwrite (slaves[nodeindex]->getfd(), write_buf);
					// forward waiting task to slave slot
					jobs[i]->schedule_task (thetask, slaves[nodeindex]);

					// turn to next job
					scheduled = true;
					scheduledjob = (i + 1)%jobs.size();
					iter--;
					break;
				}
				else     // reduce
				{
					nodeindex = thetask->peerids[0];

					if (slaves[nodeindex]->getnumrunningtasks() >= slaves[nodeindex]->getmaxreducetask())   // no available task slot
					{
						continue;
					}

					// write to the slave the task information
					stringstream ss;
					ss << "tasksubmit ";
					ss << jobs[i]->getjobid();
					ss << " ";
					ss << thetask->gettaskid();
					ss << " ";
					ss << "REDUCE";
					ss << " ";
					ss << thetask->get_job()->getargcount();

					for (int j = 0; j < thetask->get_job()->getargcount(); j++)
					{
						ss << " ";
						ss << thetask->get_job()->getargvalue (j);
					}

					string message = ss.str();
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (slaves[nodeindex]->getfd(), write_buf);
					message = SetInputPathMessage(thetask);

					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (slaves[nodeindex]->getfd(), write_buf);
					// forward waiting task to slave slot
					jobs[i]->schedule_task (thetask, slaves[nodeindex]);

					// turn to next job
					scheduled = true;
					scheduledjob = (i + 1)%jobs.size();
					iter--;
					break;
				}
			}
		}

		if(!scheduled)
		{
			// EMKDE scheduler: schedule the task where the input is located
			for (int iter = 0; (unsigned) iter < jobs.size(); iter++)
			{
				int i = (scheduledjob + iter) % jobs.size();
				int nodeindex = -1;

				if (jobs[i]->get_lastwaitingtask() == NULL)
				{
					continue;
				}

				for (int k = 0; k < jobs[i]->get_numwaiting_tasks(); k++)
				{
					master_task* thetask = jobs[i]->get_waitingtask (k);

					if (thetask->get_taskrole() == MAP)
					{
						string thepath = thetask->get_inputpath (0);   // first input as a representative input
						string address;
						stringstream tmpss;
						memset (write_buf, 0, HASHLENGTH);
						strcpy (write_buf, thepath.c_str());
						// determine the hash value and count the query
						uint32_t hashvalue = h (write_buf, HASHLENGTH);
						nodeindex = thehistogram->get_index (hashvalue);

						if (slaves[nodeindex]->getnumrunningtasks() >= slaves[nodeindex]->getmaxmaptask())     // choose alternative slot
						{
							int alternative = nodeindex;
							nodeindex = -1;
							for (int h = 0; (unsigned) h < slaves.size(); h++)
							{
								if (slaves[(alternative + h)%slaves.size()]->getnumrunningtasks() >= slaves[(alternative + h)%slaves.size()]->getmaxmaptask())
								{
									continue;
								}
								else
								{
									nodeindex = (alternative + h) % slaves.size();
									break;
								}
							}

							if (nodeindex == -1)
							{
								continue;
							}
						}

						thehistogram->count_query (hashvalue);

						// write to the slave the task information
						stringstream ss;
						ss << "tasksubmit ";
						ss << jobs[i]->getjobid();
						ss << " ";
						ss << thetask->gettaskid();
						ss << " ";
						ss << "MAP";
						ss << " ";
						ss << thetask->get_job()->getargcount();

						for (int j = 0; j < thetask->get_job()->getargcount(); j++)
						{
							ss << " ";
							ss << thetask->get_job()->getargvalue (j);
						}

						string message = ss.str();
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, message.c_str());
						nbwrite (slaves[nodeindex]->getfd(), write_buf);
						// prepare inputpath message
						int it = 0;
						message = "inputpath";

						while (it < thetask->get_numinputpaths())
						{
							if (message.length() + thetask->get_inputpath (it).length() + 1 < BUF_SIZE)
							{
								message.append (" ");
								message.append (thetask->get_inputpath (it));
							}
							else
							{
								if (thetask->get_inputpath (it).length() + 10 > BUF_SIZE)
								{
									cout << "[master]The length of inputpath exceeded the limit" << endl;
								}

								// send message to slave
								memset (write_buf, 0, BUF_SIZE);
								strcpy (write_buf, message.c_str());
								nbwrite (slaves[nodeindex]->getfd(), write_buf);
								message = "inputpath ";
								message.append (thetask->get_inputpath (it));
							}

							it++;
						}

						// send remaining paths
						if (message.length() > strlen ("inputpath "))
						{
							memset (write_buf, 0, BUF_SIZE);
							strcpy (write_buf, message.c_str());
							nbwrite (slaves[nodeindex]->getfd(), write_buf);
						}

						// notify end of inputpaths
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, "Einput");
						nbwrite (slaves[nodeindex]->getfd(), write_buf);
						// forward waiting task to slave slot
						jobs[i]->schedule_task (thetask, slaves[nodeindex]);

						// turn to next job
						scheduledjob = (i + 1)%jobs.size();
						iter--;
						break;
					}
					else     // reduce
					{
						nodeindex = thetask->peerids[0];

						if (slaves[nodeindex]->getnumrunningtasks() >= slaves[nodeindex]->getmaxreducetask())   // no available task slot
						{
							continue;
						}

						// write to the slave the task information
						stringstream ss;
						ss << "tasksubmit ";
						ss << jobs[i]->getjobid();
						ss << " ";
						ss << thetask->gettaskid();
						ss << " ";
						ss << "REDUCE";
						ss << " ";
						ss << thetask->get_job()->getargcount();

						for (int j = 0; j < thetask->get_job()->getargcount(); j++)
						{
							ss << " ";
							ss << thetask->get_job()->getargvalue (j);
						}

						string message = ss.str();
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, message.c_str());
						nbwrite (slaves[nodeindex]->getfd(), write_buf);

						message = SetInputPathMessage(thetask);

						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, message.c_str());
						nbwrite (slaves[nodeindex]->getfd(), write_buf);
						// forward waiting task to slave slot
						jobs[i]->schedule_task (thetask, slaves[nodeindex]);

						//turn to next job
						scheduledjob = (i + 1)%jobs.size();
						iter--;
						break;
					}
				}
			}
		}

		gettimeofday (&time_end, NULL);
		elapsed += 1000 * (time_end.tv_sec - time_start.tv_sec);
		elapsed += (time_end.tv_usec - time_start.tv_usec) / 1000;

		if (elapsed > UPDATEINTERVAL) // UPDATE INTERVAL from EM-KDE
		{
			// EM-KDE: calculate the new boundary according to the query counts
			thehistogram->updateboundary();
			// EM-KDE: send the boundariees of each histogram to cache server
			string message;
			stringstream ss;
			ss << "boundaries";

			for (int i = 0; i < thehistogram->get_numserver(); i++)     // total numserver - 1 boundary
			{
				ss << " ";
				ss << thehistogram->get_boundary (i);
			}

			message = ss.str();
			// send the boundary message to the cache server
			memset (write_buf, 0, BUF_SIZE);
			strcpy (write_buf, message.c_str());
			nbwrite (ipcfd, write_buf);
			gettimeofday (&time_start, NULL);
			elapsed = 0;
		}

#endif
#ifdef ACS
  int delayThreashold = 5;
  int delayCacheThreshold = 5;
  for (int i = 0; i < jobs.size(); ++i) {
    jobs[i]->update_priority();
    // cout << jobs[i]->get_priority();
  }
  for ( int i = 0; (unsigned) i < slaves.size(); i++ ) {
    if (slaves[i]->getnumrunningtasks() >= slaves[i]->getmaxmaptask())
      continue;
    std::sort(jobs.begin(), jobs.end(), jobCompLatency);
    for ( int j = 0; (unsigned) j < jobs.size(); j++ ) {
      master_task *thetask = NULL;
      int locality_lv = 999;
      for (int jj = 0; jj < jobs[j]->get_numwaiting_tasks(); ++jj) {
        master_task *atask = jobs[j]->get_waitingtask(jj);
        if (atask == NULL) continue; 
        int nodeidx = atask->dht_loc;
        string thepath;
        if (atask->get_taskrole() == MAP) {
          thepath = atask->get_inputpath (0);
        }

        if (nodeidx == i) {
          // bool fuck = atask->get_taskrole() == REDUCE;
          bool fuck = false;
          if (fuck || h_map[i].find(thepath) != h_map[i].end()) {
            thetask = atask;
            break;
          } else if (locality_lv > 1) {
            thetask = atask;
            locality_lv = 1;  // local data, not cached
          }
        } else if (jobs[j]->getdelayed() > delayCacheThreshold) {
          // bool fuck = atask->get_taskrole() == REDUCE;
          bool fuck = false;
          if (fuck || h_map[nodeidx].find(thepath) != h_map[nodeidx].end()) {
            if (locality_lv > 2) {
              thetask = atask;
              locality_lv = 2;  // remotely cached
            }
          }
        } else if (jobs[j]->getdelayed() > delayThreashold) {
          if (locality_lv > 3) {
            thetask = atask;
            locality_lv = 3; // remote data, not cached
          }
        }
      }

      if (thetask != NULL) {
        if (thetask->get_taskrole() == MAP) {
          jobs[j]->sched();
          stringstream ss;
          ss << "tasksubmit ";
          ss << jobs[j]->getjobid();
          ss << " ";
          ss << thetask->gettaskid();
          ss << " ";
          ss << "MAP";
          ss << " ";
          ss << thetask->get_job()->getargcount();

          for (int k = 0; k < thetask->get_job()->getargcount(); k++)
          {
            ss << " ";
            ss << thetask->get_job()->getargvalue (k);
          }
          string message = ss.str();
          memset (write_buf, 0, BUF_SIZE);
          strcpy (write_buf, message.c_str());
          nbwrite (slaves[i]->getfd(), write_buf);
          // prepare inputpath message
          int iter = 0;
          message = "inputpath";

          while (iter < thetask->get_numinputpaths())
          {
//							cout << jobs[j]->getjobid() << endl;
//							cout << thetask->get_inputpath(iter) << endl;
            if (message.length() + thetask->get_inputpath (iter).length() + 1 < BUF_SIZE)
            {
              message.append (" ");
              message.append (thetask->get_inputpath (iter));
            }
            else
            {
              if (thetask->get_inputpath (iter).length() + 10 > BUF_SIZE)
              {
                cout << "[master]The length of inputpath exceeded the limit" << endl;
              }

              // send message to slave
              memset (write_buf, 0, BUF_SIZE);
              strcpy (write_buf, message.c_str());
              nbwrite (slaves[i]->getfd(), write_buf);
              message = "inputpath ";
              message.append (thetask->get_inputpath (iter));
            }

            iter++;
          }

          // send remaining paths
          if (message.length() > strlen ("inputpath "))
          {
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf, message.c_str());
            nbwrite (slaves[i]->getfd(), write_buf);
          }

          // notify end of inputpaths
          memset (write_buf, 0, BUF_SIZE);
          strcpy (write_buf, "Einput");
          nbwrite (slaves[i]->getfd(), write_buf);
          // forward waiting task to slave slot
          jobs[j]->schedule_task (thetask, slaves[i]);

          if (true) {
            if (thetask->get_taskrole() == MAP) {
              string input_path = thetask->get_inputpath(0);
              int cidx = thetask->dht_loc;
              if (h_map[cidx].find(input_path) == h_map[cidx].end()) {
                file_history[cidx].emplace_back(input_path);
                auto h_it = file_history[cidx].end();
                --h_it;
                h_map[cidx].emplace(input_path, h_it);
                if (h_map[cidx].size() > 40) {
                  h_map[cidx].erase(file_history[cidx].front());
                  file_history[cidx].pop_front();
                }
              } else {
                file_history[cidx].erase(h_map[cidx][input_path]);
                file_history[cidx].emplace_back(input_path);
                auto h_it = file_history[cidx].end();
                --h_it;
                h_map[cidx][input_path] = h_it;
              }
            }
          }
          // sleeps for 0.0001 seconds. change this if necessary
          // usleep(100000);
          break;
        } else {
          jobs[j]->sched();
          stringstream ss;
          ss << "tasksubmit ";
          ss << jobs[j]->getjobid();
          ss << " ";
          ss << thetask->gettaskid();
          ss << " ";
          ss << "REDUCE";
          ss << " ";
          ss << thetask->get_job()->getargcount();

          for (int k = 0; k < thetask->get_job()->getargcount(); k++)
          {
            ss << " ";
            ss << thetask->get_job()->getargvalue (k);
          }
          string message = ss.str();
          memset (write_buf, 0, BUF_SIZE);
          strcpy (write_buf, message.c_str());
          nbwrite (slaves[i]->getfd(), write_buf);
          // prepare inputpath message
          message = SetInputPathMessage(thetask);

          // notify end of inputpaths
          memset (write_buf, 0, BUF_SIZE);
          strcpy (write_buf, message.c_str());
          nbwrite (slaves[i]->getfd(), write_buf);
          // forward waiting task to slave slot
          jobs[j]->schedule_task (thetask, slaves[i]);
          // sleeps for 0.0001 seconds. change this if necessary
          // usleep(100000);
          break;
        }
      } else { 
        jobs[j]->delayed((int)std::time(NULL));
      }// thetask == NULL
    }  // j: jobs
  }

  // for (int i = 0; i < slaves.size() ; ++i) {
  //   if (slaves[i]->getnumrunningtasks() >= slaves[i]->getmaxmaptask()) continue;
  //   master_task *thetask = NULL;
  //   list<master_task*>::iterator theit;
  //   int thejob;
  //   for (int ii = 0; ii < jobs.size(); ++ii) {
  //     for (auto it = local_tasks[ii][i].begin(); it != local_tasks[ii][i].end();
  //         ++it) {
  //       auto atask = *it;
  //       if (latency_aware[i] != 0 && ii > 0) {
  //         ii = jobs.size();
  //         break;
  //       }
  //       if (atask == NULL) continue;
  //       if (thetask == NULL) {
  //         if (atask->get_taskrole() == MAP) {
  //           if (h_map[i].find(atask->get_inputpath(0)) != h_map[i].end()) {
  //             thetask = atask;
  //             theit = it;
  //             thejob = ii;
  //             ii = jobs.size();
  //             break;
  //           } else {
  //             thetask = atask;
  //             theit = it;
  //             thejob = ii;
  //           }
  //         } else {
  //           thetask = atask;
  //           theit = it;
  //           thejob = ii;
  //           ii = jobs.size();
  //           break;
  //         }
  //       } else {
  //         if (atask->get_taskrole() == MAP) {
  //           if (h_map[i].find(atask->get_inputpath(0)) != h_map[i].end()) {
  //             if (latency_aware[i] == 0 ||
  //                 thetask->get_job()->getjobid() == atask->get_job()->getjobid()) {
  //               thetask = atask;
  //               theit = it;
  //               thejob = ii;
  //               ii = jobs.size();
  //               break;
  //             }
  //           }
  //         }
  //       }
  //     }
  //   }
  //   if (thetask != NULL) {
  //     // local_tasks[i].remove(thetask);
  //     local_tasks[thejob][i].erase(theit);
  //   } else {
  //     for (int ii = 0; ii < jobs.size(); ++ii) {
  //       bool iswaitingt = false;
  //       int maxservidx = i;
  //       for (int j = 0; j < slaves.size(); ++j) {
  //         if (i == j) continue;
  //         if (local_tasks[ii][j].size() > local_tasks[ii][maxservidx].size()) {
  //           iswaitingt = true;
  //           maxservidx = j;
  //           break;
  //         }
  //       }
  //       if (iswaitingt) {
  //             thetask = local_tasks[ii][maxservidx].front();
  //             local_tasks[ii][maxservidx].pop_front();
  //             thejob = ii;
  //             ii = jobs.size();
  //             break;
  //       }
  //     }
  //     /*
  //     for (int j = 0; j < slaves.size(); ++j) {
  //       if (i == j) continue;
  //       if (local_tasks[j].size() > 0) {
  //         thetask = local_tasks[j].front();
  //       }
  //       for (auto it = local_tasks[j].begin(); it != local_tasks[j].end(); ++it) {
  //         auto atask = *it;
  //         if (latency_aware[i] != 0 &&
  //             thetask->get_job()->getjobid() < atask->get_job()->getjobid()) {
  //           break;
  //         }
  //         if (atask->get_taskrole() == MAP) {
  //           if (h_map[j].find(atask->get_inputpath(0)) != h_map[j].end()) {
  //             if (latency_aware[i] == 0 ||
  //                 thetask->get_job()->getjobid() == atask->get_job()->getjobid()) {
  //               thetask = atask;
  //               break;
  //             }
  //           }
  //         }   
  //       }
  //       if (thetask != NULL) {
  //         local_tasks[j].remove(thetask);
  //         break;
  //       }
  //     }
  //     */
  //   }
  //   if (thetask != NULL) {
  //     int j = thejob;
  //     if (thetask->get_taskrole() == MAP) {
  //       jobs[j]->sched();
  //       stringstream ss;
  //       ss << "tasksubmit ";
  //       ss << jobs[j]->getjobid();
  //       ss << " ";
  //       ss << thetask->gettaskid();
  //       ss << " ";
  //       ss << "MAP";
  //       ss << " ";
  //       ss << thetask->get_job()->getargcount();

  //       for (int k = 0; k < thetask->get_job()->getargcount(); k++)
  //       {
  //         ss << " ";
  //         ss << thetask->get_job()->getargvalue (k);
  //       }
  //       string message = ss.str();
  //       memset (write_buf, 0, BUF_SIZE);
  //       strcpy (write_buf, message.c_str());
  //       nbwrite (slaves[i]->getfd(), write_buf);
  //       // prepare inputpath message
  //       int iter = 0;
  //       message = "inputpath";

  //       while (iter < thetask->get_numinputpaths())
  //       {
//// 							cout << jobs[j]->getjobid() << endl;
//// 							cout << thetask->get_inputpath(iter) << endl;
  //         if (message.length() + thetask->get_inputpath (iter).length() + 1 < BUF_SIZE)
  //         {
  //           message.append (" ");
  //           message.append (thetask->get_inputpath (iter));
  //         }
  //         else
  //         {
  //           if (thetask->get_inputpath (iter).length() + 10 > BUF_SIZE)
  //           {
  //             cout << "[master]The length of inputpath exceeded the limit" << endl;
  //           }

  //           // send message to slave
  //           memset (write_buf, 0, BUF_SIZE);
  //           strcpy (write_buf, message.c_str());
  //           nbwrite (slaves[i]->getfd(), write_buf);
  //           message = "inputpath ";
  //           message.append (thetask->get_inputpath (iter));
  //         }

  //         iter++;
  //       }

  //       // send remaining paths
  //       if (message.length() > strlen ("inputpath "))
  //       {
  //         memset (write_buf, 0, BUF_SIZE);
  //         strcpy (write_buf, message.c_str());
  //         nbwrite (slaves[i]->getfd(), write_buf);
  //       }

  //       // notify end of inputpaths
  //       memset (write_buf, 0, BUF_SIZE);
  //       strcpy (write_buf, "Einput");
  //       nbwrite (slaves[i]->getfd(), write_buf);
  //       // forward waiting task to slave slot
  //       string input_path = thetask->get_inputpath(0);
  //       if (h_map[i].find(input_path) == h_map[i].end()) {
  //         h_map[i].emplace(input_path, true);
  //         file_history[i].emplace_back(input_path);
  //         if (h_map[i].size() > 48) {
  //           h_map[i].erase(file_history[i].front());
  //           file_history[i].pop_front();
  //         }
  //       } else {
  //         file_history[i].remove(input_path);
  //         file_history[i].emplace_back(input_path);
  //       }
  //       jobs[j]->schedule_task_batch(thetask, slaves[i]);
  //       // sleeps for 0.0001 seconds. change this if necessary
  //       // usleep(100000);
  //     } else {
  //       jobs[j]->sched();
  //       stringstream ss;
  //       ss << "tasksubmit ";
  //       ss << jobs[j]->getjobid();
  //       ss << " ";
  //       ss << thetask->gettaskid();
  //       ss << " ";
  //       ss << "REDUCE";
  //       ss << " ";
  //       ss << thetask->get_job()->getargcount();

  //       for (int k = 0; k < thetask->get_job()->getargcount(); k++)
  //       {
  //         ss << " ";
  //         ss << thetask->get_job()->getargvalue (k);
  //       }
  //       string message = ss.str();
  //       memset (write_buf, 0, BUF_SIZE);
  //       strcpy (write_buf, message.c_str());
  //       nbwrite (slaves[i]->getfd(), write_buf);
  //       // prepare inputpath message
  //       message = SetInputPathMessage(thetask);

  //       // notify end of inputpaths
  //       memset (write_buf, 0, BUF_SIZE);
  //       strcpy (write_buf, message.c_str());
  //       nbwrite (slaves[i]->getfd(), write_buf);
  //       // forward waiting task to slave slot
  //       jobs[j]->schedule_task_batch(thetask, slaves[i]);
  //       // sleeps for 0.0001 seconds. change this if necessary
  //       // usleep(100000);
  //     }
  //     latency_aware[i] = (latency_aware[i] + 1) % (num_la + 1);
  //   } else {
  //     /*
  //     if (jobs.size() > 0) {
  //       cout << "Nothing scheduled on slave " << i + 1<< endl;
  //       for (int l = 0; l < jobs.size(); ++l) {
  //         cout << "Job " << l << " : ";
  //         for (int ll = 0; ll < slaves.size(); ++ll) {
  //           cout << local_tasks[l][ll].size() << " ";
  //         }
  //         cout << endl;
  //       }
  //     }
  //     */
  //   }
  // }
#endif

#ifdef NEW_LCA
  int delayThreashold = 5;
  for (int i = 0; i < jobs.size(); ++i) {
    for (int j = 0; j < jobs[i]->get_numwaiting_tasks(); ++j) {
      auto thetask = jobs[i]->get_waitingtask(j);
      int k;
      if (thetask->get_taskrole() == MAP) {
        string thepath = thetask->get_inputpath(0);
        memset(write_buf, 0, HASHLENGTH);
        strcpy(write_buf, thepath.c_str());
        uint32_t hash_value = h(write_buf, HASHLENGTH);
        k = hash_value % slaves.size();
      } else {
        k = thetask->peerids[0];
      }
      local_tasks[i][k].push_back(thetask);
      jobs[i]->queue(j);
      --j;
    }
  }

  for (int i = 0; i < slaves.size() ; ++i) {
    if (slaves[i]->getnumrunningtasks() >= slaves[i]->getmaxmaptask()) continue;
    master_task *thetask = NULL;
    list<master_task*>::iterator theit;
    int thejob;
    for (int ii = 0; ii < jobs.size(); ++ii) {
      for (auto it = local_tasks[ii][i].begin(); it != local_tasks[ii][i].end();
          ++it) {
        auto atask = *it;
        if (latency_aware[i] != 0 && ii > 0) {
          ii = jobs.size();
          break;
        }
        if (atask == NULL) continue;
        if (thetask == NULL) {
          if (atask->get_taskrole() == MAP) {
            if (h_map[i].find(atask->get_inputpath(0)) != h_map[i].end()) {
              thetask = atask;
              theit = it;
              thejob = ii;
              ii = jobs.size();
              break;
            } else {
              thetask = atask;
              theit = it;
              thejob = ii;
            }
          } else {
            thetask = atask;
            theit = it;
            thejob = ii;
            ii = jobs.size();
            break;
          }
        } else {
          if (atask->get_taskrole() == MAP) {
            if (h_map[i].find(atask->get_inputpath(0)) != h_map[i].end()) {
              if (latency_aware[i] == 0 ||
                  thetask->get_job()->getjobid() == atask->get_job()->getjobid()) {
                thetask = atask;
                theit = it;
                thejob = ii;
                ii = jobs.size();
                break;
              }
            }
          }
        }
      }
    }
    if (thetask != NULL) {
      // local_tasks[i].remove(thetask);
      local_tasks[thejob][i].erase(theit);
    } else {
      for (int ii = 0; ii < jobs.size(); ++ii) {
        bool iswaitingt = false;
        int maxservidx = i;
        for (int j = 0; j < slaves.size(); ++j) {
          if (i == j) continue;
          if (local_tasks[ii][j].size() > local_tasks[ii][maxservidx].size()) {
            iswaitingt = true;
            maxservidx = j;
            break;
          }
        }
        if (iswaitingt) {
              thetask = local_tasks[ii][maxservidx].front();
              local_tasks[ii][maxservidx].pop_front();
              thejob = ii;
              ii = jobs.size();
              break;
        }
      }
      /*
      for (int j = 0; j < slaves.size(); ++j) {
        if (i == j) continue;
        if (local_tasks[j].size() > 0) {
          thetask = local_tasks[j].front();
        }
        for (auto it = local_tasks[j].begin(); it != local_tasks[j].end(); ++it) {
          auto atask = *it;
          if (latency_aware[i] != 0 &&
              thetask->get_job()->getjobid() < atask->get_job()->getjobid()) {
            break;
          }
          if (atask->get_taskrole() == MAP) {
            if (h_map[j].find(atask->get_inputpath(0)) != h_map[j].end()) {
              if (latency_aware[i] == 0 ||
                  thetask->get_job()->getjobid() == atask->get_job()->getjobid()) {
                thetask = atask;
                break;
              }
            }
          }   
        }
        if (thetask != NULL) {
          local_tasks[j].remove(thetask);
          break;
        }
      }
      */
    }
    if (thetask != NULL) {
      int j = thejob;
      if (thetask->get_taskrole() == MAP) {
        jobs[j]->sched();
        stringstream ss;
        ss << "tasksubmit ";
        ss << jobs[j]->getjobid();
        ss << " ";
        ss << thetask->gettaskid();
        ss << " ";
        ss << "MAP";
        ss << " ";
        ss << thetask->get_job()->getargcount();

        for (int k = 0; k < thetask->get_job()->getargcount(); k++)
        {
          ss << " ";
          ss << thetask->get_job()->getargvalue (k);
        }
        string message = ss.str();
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, message.c_str());
        nbwrite (slaves[i]->getfd(), write_buf);
        // prepare inputpath message
        int iter = 0;
        message = "inputpath";

        while (iter < thetask->get_numinputpaths())
        {
//							cout << jobs[j]->getjobid() << endl;
//							cout << thetask->get_inputpath(iter) << endl;
          if (message.length() + thetask->get_inputpath (iter).length() + 1 < BUF_SIZE)
          {
            message.append (" ");
            message.append (thetask->get_inputpath (iter));
          }
          else
          {
            if (thetask->get_inputpath (iter).length() + 10 > BUF_SIZE)
            {
              cout << "[master]The length of inputpath exceeded the limit" << endl;
            }

            // send message to slave
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf, message.c_str());
            nbwrite (slaves[i]->getfd(), write_buf);
            message = "inputpath ";
            message.append (thetask->get_inputpath (iter));
          }

          iter++;
        }

        // send remaining paths
        if (message.length() > strlen ("inputpath "))
        {
          memset (write_buf, 0, BUF_SIZE);
          strcpy (write_buf, message.c_str());
          nbwrite (slaves[i]->getfd(), write_buf);
        }

        // notify end of inputpaths
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, "Einput");
        nbwrite (slaves[i]->getfd(), write_buf);
        // forward waiting task to slave slot
        string input_path = thetask->get_inputpath(0);
        if (h_map[i].find(input_path) == h_map[i].end()) {
          h_map[i].emplace(input_path, true);
          file_history[i].emplace_back(input_path);
          if (h_map[i].size() > 48) {
            h_map[i].erase(file_history[i].front());
            file_history[i].pop_front();
          }
        } else {
          file_history[i].remove(input_path);
          file_history[i].emplace_back(input_path);
        }
        jobs[j]->schedule_task_batch(thetask, slaves[i]);
        // sleeps for 0.0001 seconds. change this if necessary
        // usleep(100000);
      } else {
        jobs[j]->sched();
        stringstream ss;
        ss << "tasksubmit ";
        ss << jobs[j]->getjobid();
        ss << " ";
        ss << thetask->gettaskid();
        ss << " ";
        ss << "REDUCE";
        ss << " ";
        ss << thetask->get_job()->getargcount();

        for (int k = 0; k < thetask->get_job()->getargcount(); k++)
        {
          ss << " ";
          ss << thetask->get_job()->getargvalue (k);
        }
        string message = ss.str();
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, message.c_str());
        nbwrite (slaves[i]->getfd(), write_buf);
        // prepare inputpath message
        message = SetInputPathMessage(thetask);

        // notify end of inputpaths
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, message.c_str());
        nbwrite (slaves[i]->getfd(), write_buf);
        // forward waiting task to slave slot
        jobs[j]->schedule_task_batch(thetask, slaves[i]);
        // sleeps for 0.0001 seconds. change this if necessary
        // usleep(100000);
      }
      latency_aware[i] = (latency_aware[i] + 1) % (num_la + 1);
    } else {
      /*
      if (jobs.size() > 0) {
        cout << "Nothing scheduled on slave " << i + 1<< endl;
        for (int l = 0; l < jobs.size(); ++l) {
          cout << "Job " << l << " : ";
          for (int ll = 0; ll < slaves.size(); ++ll) {
            cout << local_tasks[l][ll].size() << " ";
          }
          cout << endl;
        }
      }
      */
    }
  }
#endif
#ifdef LCA
/*
for (int i = 0; i < slaves.size(); ++i) {
  cout << "Slave " << i << endl;
  if (task_queue_it[i] != task_queue[i]->end()) {
    cout << "it->input: " << (*task_queue_it[i])->get_inputpath(0) << endl;
  }
  for (auto j = task_queue[i]->begin(); j != task_queue[i]->end(); ++j) {
    cout << (*j)->get_inputpath(0) << " ";
  }
  cout << endl;
}
sleep(5);
*/
  for (int ii = 0; ii < slaves.size(); ++ii) {
    int i = (slave_iter + ii) % slaves.size();
    if ((slaves[i]->getnumrunningtasks() < slaves[i]->getmaxmaptask())
        && (task_queue_it[i] != task_queue[i]->end())) {
      master_task *thetask = *task_queue_it[i];
      stringstream ss;
      ss << "tasksubmit ";
      ss << thetask->get_job()->getjobid();
      ss << " ";
      ss << thetask->gettaskid();
      if (thetask->get_taskrole() == MAP) {
        ss << " MAP ";
      } else {
        ss << " REDUCE ";
      }
      ss << thetask->get_job()->getargcount();
      for (int j = 0; j < thetask->get_job()->getargcount(); ++j) {
        ss << " ";
        ss << thetask->get_job()->getargvalue(j);
      }
      string message = ss.str();
      memset(write_buf, 0, BUF_SIZE);
      strcpy(write_buf, message.c_str());
      nbwrite(slaves[i]->getfd(), write_buf);
      if (thetask->get_taskrole() == MAP) {
        int it = 0;
				message = "inputpath";
        while (it < thetask->get_numinputpaths()) {
          if (message.length() + thetask->get_inputpath(it).length() + 1 <
              BUF_SIZE) {
            message.append (" ");
            message.append (thetask->get_inputpath(it));
          } else {
            if (thetask->get_inputpath (it).length() + 10 > BUF_SIZE) {
              cout << "[master]The length of inputpath exceeded the limit"
                << endl;
            }
            // send message to slave
            memset (write_buf, 0, BUF_SIZE);
            strcpy (write_buf, message.c_str());
            nbwrite (slaves[i]->getfd(), write_buf);
            message = "inputpath ";
            message.append (thetask->get_inputpath (it));
          }
          it++;
        }
        // send remaining paths
        if (message.length() > strlen ("inputpath ")) {
          memset (write_buf, 0, BUF_SIZE);
          strcpy (write_buf, message.c_str());
          nbwrite (slaves[i]->getfd(), write_buf);
        }
        // notify end of inputpaths
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, "Einput");
        nbwrite (slaves[i]->getfd(), write_buf);
        string input_path = thetask->get_inputpath(0);
        if (h_map[i].find(input_path) == h_map[i].end()) {
          file_history[i].emplace_back(input_path);
          auto h_it = file_history[i].end();
          --h_it;
          h_map[i].emplace(input_path, h_it);
          if (h_map[i].size() > 40) {
            h_map[i].erase(file_history[i].front());
            file_history[i].pop_front();
          }
        } else {
          file_history[i].erase(h_map[i][input_path]);
          file_history[i].emplace_back(input_path);
          auto h_it = file_history[i].end();
          --h_it;
          h_map[i][input_path] = h_it;
        }
      } else {
        message = SetInputPathMessage(thetask);
        memset(write_buf, 0, BUF_SIZE);
        strcpy(write_buf, message.c_str());
        nbwrite(slaves[i]->getfd(), write_buf);
      }
      thetask->get_job()->schedule_task_batch(thetask, slaves[i]);
      // cout << "scheduled input: " << thetask->get_inputpath(0) << endl;
      ++task_queue_it[i];
      if (task_queue_it[i] == task_queue[i]->end()) {
        if (!next_task_queue[i]->empty()) {
          delete task_queue[i];
          task_queue[i] = next_task_queue[i];
          task_queue_it[i] = task_queue[i]->begin();
          next_task_queue[i] = new list<master_task*>();
          latency_aware[i] = (latency_aware[i] + 1) % (num_la + 1);
        }
        all_set = false;
      }
      /*
      if ((task_queue_it[i] == task_queue[i]->end())
          && (!next_task_queue[i]->empty())) {
        // delete last_task_queue[i];
        // last_task_queue[i] = task_queue[i];
        delete task_queue[i];
        task_queue[i] = next_task_queue[i];
        task_queue_it[i] = task_queue[i]->begin();
        next_task_queue[i] = new list<master_task*>();
        delete cmap[i];
        cmap[i] = next_cmap[i];
        next_cmap[i] = new unordered_map<string, bool>();
        all_set = false;
      }
      */
      // slave_iter = (i + 1) % slaves.size();
    }
  }

  const int kbatch_size = 8;

  if (false) {
    for (int i = 0; i < slaves.size(); ++i) {
      if (next_task_queue[i]->empty()) {
        num_replacable[i] = -kbatch_size / 2;
      }
    }
    for (int i = 0; i < jobs.size(); ++i) {
      for (int j = 0; j < jobs[i]->get_numwaiting_tasks(); ++j) {
        auto thetask = jobs[i]->get_waitingtask(j);
        if (thetask->get_taskrole() == MAP) {
          string thepath = thetask->get_inputpath(0);
          memset(write_buf, 0, HASHLENGTH);
          strcpy(write_buf, thepath.c_str());
          uint32_t hash_value = h(write_buf, HASHLENGTH);
          int k = hash_value % slaves.size();
          if (next_task_queue[k]->size() < kbatch_size) {
            jobs[i]->queue(j);
            next_task_queue[k]->push_back(thetask);
            --j;
          }
        }
      }
    }
  }
  
  if (!all_set) {
    for (int i = 0; i < slaves.size(); ++i) {
      if (next_task_queue[i]->empty()) {
        num_replacable[i] = kbatch_size;
      }
    }

    for (int i = 0; i < jobs.size(); ++i) {
      for (int j = 0; j < jobs[i]->get_numwaiting_tasks(); ++j) {
        auto thetask = jobs[i]->get_waitingtask(j);
        if (thetask == NULL) continue;
        if (thetask->get_taskrole() == MAP) {
          string thepath = thetask->get_inputpath(0);
          memset(write_buf, 0, HASHLENGTH);
          strcpy(write_buf, thepath.c_str());
          uint32_t hash_value = h(write_buf, HASHLENGTH);
          int k = hash_value % slaves.size();
          if (next_task_queue[k]->size() < kbatch_size) {
            if (h_map[k].find(thepath) != h_map[k].end()) {
              --num_replacable[k];
              next_task_queue[k]->push_back(thetask);
            } else {
              next_task_queue[k]->push_front(thetask);
            }
            jobs[i]->queue(j);
            --j;
          }
        } else {
          int k = thetask->peerids[0];
          if (next_task_queue[k]->size() < kbatch_size) {
            jobs[i]->queue(j);
            --num_replacable[k];
            next_task_queue[k]->push_back(thetask);
            --j;
          }
        }
      }
    }
    
    for (int i = 0; i < jobs.size(); ++i) {
      for (int j = 0; j < jobs[i]->get_numwaiting_tasks(); ++j) {
        auto thetask = jobs[i]->get_waitingtask(j);
        if (thetask == NULL) continue;
        if (thetask->get_taskrole() == MAP) {
          string thepath = thetask->get_inputpath(0);
          memset(write_buf, 0, HASHLENGTH);
          strcpy(write_buf, thepath.c_str());
          uint32_t hash_value = h(write_buf, HASHLENGTH);
          int k = hash_value % slaves.size();
          if (next_task_queue[k]->size() < kbatch_size) {
            jobs[i]->queue(j);
            next_task_queue[k]->push_back(thetask);
            --j;
          } else if (latency_aware[k] == 0 && num_replacable[k] > 0) {
            if (h_map[k].find(thepath) != h_map[k].end()) {
              jobs[i]->queue(j);
              jobs[i]->withdraw(next_task_queue[k]->front());
              next_task_queue[k]->pop_front();
              next_task_queue[k]->push_back(thetask);
              --num_replacable[k];
            }
          }
        }
      }
    }
    all_set = true;
  }
  for (int i = 0; i < slaves.size(); ++i) {
    if (task_queue_it[i] == task_queue[i]->end()) {
      if (!next_task_queue[i]->empty()) {
        delete task_queue[i];
        task_queue[i] = next_task_queue[i];
        task_queue_it[i] = task_queue[i]->begin();
        next_task_queue[i] = new list<master_task*>();
        latency_aware[i] = (latency_aware[i] + 1) % (num_la + 1);
      }
      all_set = false;
    } else if (next_task_queue[i]->size() < kbatch_size) {
      all_set = false;
    }
  }

  if (false) {
    for (int i = 0; i < slaves.size(); ++i) {
      if (next_task_queue[i]->empty()) {
        num_replacable[i] = -kbatch_size / 2;
      }
    }
    for (int i = 0; i < jobs.size(); ++i) {
      for (int j = 0; j < jobs[i]->get_numwaiting_tasks(); ++j) {
        auto thetask = jobs[i]->get_waitingtask(j);
        if (thetask == NULL) continue;
        if (thetask->get_taskrole() == MAP) {
          string thepath = thetask->get_inputpath(0);
          memset(write_buf, 0, HASHLENGTH);
          strcpy(write_buf, thepath.c_str());
          uint32_t hash_value = h(write_buf, HASHLENGTH);
          int k = hash_value % slaves.size();
          if (next_task_queue[k]->size() == kbatch_size) {
            if (next_task_queue[k]->front()->get_job()->getjobid()
                == jobs[i]->getjobid()) {
              if (num_replacable[k] == -kbatch_size / 2) continue;
            } else {
              if (num_replacable[k] == 0) continue;
            }
          }
          if (cmap[k]->find(thepath) != cmap[k]->end()) {
            if (next_task_queue[k]->size() < kbatch_size) {
              jobs[i]->queue(j);
              next_task_queue[k]->push_back(thetask);
              next_cmap[k]->emplace(thepath, true);
              --j;
            } else if (((next_task_queue[k]->front()->get_job()->getjobid()
                == jobs[i]->getjobid())
                && (num_replacable[k] > -kbatch_size / 2))
                || (num_replacable[k] > 0)) {
              jobs[i]->queue(j);
              jobs[i]->withdraw(next_task_queue[k]->front());
              next_cmap[k]->erase(
                  next_task_queue[k]->front()->get_inputpath(0));
              next_task_queue[k]->pop_front();
              next_task_queue[k]->push_back(thetask);
              next_cmap[k]->emplace(thepath, true);
              --num_replacable[k];
            } else {
              jobs[i]->queue(j);
              jobs[i]->withdraw(next_task_queue[k]->front());
              next_cmap[k]->erase(
                  next_task_queue[k]->front()->get_inputpath(0));
              next_task_queue[k]->pop_front();
              next_task_queue[k]->push_back(thetask);
              next_cmap[k]->emplace(thepath, true);
              --num_replacable[k];
            }
          } else {
            if (next_task_queue[k]->size() < kbatch_size) {
              jobs[i]->queue(j);
              next_task_queue[k]->push_front(thetask);
              next_cmap[k]->emplace(thepath, true);
              ++num_replacable[k];
              --j;
            }
          }
          /*
          if (next_task_queue[k]->size() < kbatch_size) {
            jobs[i]->queue(j);
            next_task_queue[k]->push_front(thetask);
          }
          */
        } else {
          int k = thetask->peerids[0];
          if (next_task_queue[k]->size() < kbatch_size) {
            jobs[i]->queue(j);
            next_task_queue[k]->push_back(thetask);
            ++num_replacable[k];
            --j;
          }
        }
      }
    }
    all_set = true;
    for (int i = 0; i < slaves.size(); ++i) {
      if (task_queue_it[i] == task_queue[i]->end()) {
        if (!next_task_queue[i]->empty()) {
          // delete last_task_queue[i];
          // last_task_queue[i] = task_queue[i];
          delete task_queue[i];
          task_queue[i] = next_task_queue[i];
          task_queue_it[i] = task_queue[i]->begin();
          next_task_queue[i] = new list<master_task*>();
          delete cmap[i];
          cmap[i] = next_cmap[i];
          next_cmap[i] = new unordered_map<string, bool>();
        }
        all_set = false;
      }
    }
    // for (int i = 0; i < slaves.size(); ++i) {
    //   if ((next_task_queue[i]->size() < 64) || (num_replacable[i] > 0)) {
    //     all_set = false;
    //   }
    // }
  }
#endif

		// break if all slaves and clients are closed
		if (slaves.size() == 0 && clients.size() == 0)
		{
			break;
		}

		// sleeps for 1 msec. change this if necessary
		// usleep(1000);
	}

	// close master socket
	close (serverfd);
	cout << "[master]Master server closed" << endl;
	cout << "[master]Exiting master..." << endl;
	thread_continue = false;
	return NULL;
}

master_job* find_jobfromid (int id)
{
	for (int i = 0; (unsigned) i < jobs.size(); i++)
	{
		if (jobs[i]->getjobid() == id)
		{
			return jobs[i];
		}
	}

	return NULL;
}

string SetInputPathMessage (master_task *thetask) {
	string message = "inputpath";

	for (int i = 0; (unsigned) i < thetask->peerids.size(); ++i) {
	// don't need to worry about BUF_SIZE overflow in reducer case
		stringstream ss;
		ss << " ";
		ss << thetask->peerids[i];
		ss << " ";
		ss << thetask->numiblocks[i];
		ss << " ";
		ss << thetask->threadids[i];
		message.append (ss.str());
	}

	return message;
}
