#include "master.hh"

#include "../common/ecfs.hh"
#include "../common/histogram.hh"
#include "../common/logger.hh"
#include "master_job.hh"
#include "master_task.hh"
#include "connslave.hh"
#include "connclient.hh"
#include "cacheclient.hh"
#include "iwfrequest.hh"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pthread.h>

// mpi
#include <mpi.h>
#include "../slave/slave.hh"

using namespace std;

vector<connslave*> slaves;
vector<connclient*> clients;
vector<cacheclient*> cache_clients;
vector<master_job*> jobs;
vector<string> nodelist;
histogram* thehistogram;
vector<iwfrequest*> iwfrequests;

int serverfd;
int max_job = -1;
int jobidclock = 0; // job id starts 0

Logger* log; // instead of cout, we use log

char read_buf[BUF_SIZE]; // read buffer for signal_listener thread
char write_buf[BUF_SIZE]; // write buffer for signal_listener thread

// --------------------------master protocol---------------------------------
// 1. close: let the destination node close the connection from the master
// 2. ignored: let connected node know message from it was ignored
// 3. result: contains resulting messeage to client followed by 'result:'
// --------------------------------------------------------------------------
// TODO: make protocols to integer or enum

int main (int argc, char** argv)
{
	// load settings
	Settings setted;
	setted.load();
	max_job         = setted.get<int> ("max_job");
	nodelist        = setted.get<vector<string> > ("network.nodes");
	int connectioncount = 0;
	
	// set log
	string logname  = setted.get<string> ("log.name");
	string logtype  = setted.get<string> ("log.type");
	log             = Logger::connect(logname, logtype);

	// initialize mpi
	int myrank, numprocs;
	MPI_Status status;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	if(myrank == 0) // master node
	{
		// set socket network
		string token;
		int port        = setted.get<int> ("network.port_mapreduce");

		// initialize the EM-KDE histogram
		thehistogram = new histogram (nodelist.size(), NUMBIN);

		// open server
		open_server (port);
		if (serverfd < 0)
		{
			log->error (" Openning server failed");
			return EXIT_FAILURE;
		}

		// set sockets to be non-blocking socket to avoid deadlock
		fcntl (serverfd, F_SETFL, O_NONBLOCK);

		// communicate with slaves using mpi
		void *tret;
		pthread_t mpi_thread;
		pthread_create (&mpi_thread, NULL, mpi_master_listener, NULL);

		// communicate with clients/jobs/cache_server using socket
		socket_listener(serverfd);

		// wait until mpi thread is done
		pthread_join(mpi_thread, &tret);
	}
	else // slave nodes
	{
		launch_slave(myrank);
	}

	Logger::disconnect (log);
	MPI_Finalize();
	return EXIT_SUCCESS;
}

void open_server (int port)
{
    struct sockaddr_in serveraddr;
    // socket open
    serverfd = socket (AF_INET, SOCK_STREAM, 0);
    
    if (serverfd < 0)
    {
        log->info ("Socket opening failed");
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
        log->info ("Binding failed");
        exit (1);
    }
    
    // listen
    if (listen (serverfd, BACKLOG) < 0)
    {
        log->info ("Listening failed");
        exit (1);
    }
}

void launch_slave (int rank)
{
	// push slave to vector
	connslave tmpslave(rank);
	tmpslave.setmaxmaptask (MAP_SLOT);
	tmpslave.setmaxreducetask (REDUCE_SLOT);
	slaves.push_back (tmpslave);
	// push cache_client to vector
	cacheclient tmpcache(rank);
	cache_clients.push_back (tmpcache);
	connectioncount++;
    log->info  ("[%d/%d] slave rank %d is launched", connectioncount, nodelist.size(), rank);

	if ( (unsigned) connectioncount == nodelist.size())
	{
		log->info ("All slave nodes are connected successfully");
	}
	else if (slaves.size() > nodelist.size())
	{
		log->warn ("Number of slave connection exceeded allowed limits");
		log->warn ("Debugging needed on this problem");
	}
	
	// initialize pthread variables
	pthread_t cache_thread;
	void *tret;
	pthread_create(&cache_thread, NULL, cache_client, NULL);
	mpi_slave_listener(rank);

	pthread_join(cache_thread, &tret);
}

void mpi_master_listener (int rank)
{
	// mpi network
	//MPI_Request reqest;
	int readbytes = 0; // bytes of read message
	int tmprank = -1; // store rank of new connected node temporarily
	int elapsed = 0; // time elapsed im msec
	struct timeval time_start;
	struct timeval time_end;
	gettimeofday (&time_start, NULL);
	gettimeofday (&time_end, NULL);

	// listen signals from slave nodes
	while (1)
	{
		// blocking read to check identification of connected node
		memset (read_buf, 0, BUF_SIZE);
		MPI_Recv(&read_buf, BUF_SIZE, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		tmprank = status.MPI_SOURCE;
		MPI_Get_count(&status, MPI_CHAR, &readbytes);

		if (readbytes == 0)     // connection closed from slave
		{
			log->info("Connection from a slave closed\n");
			for(int i = 0; (unsigned) i < slaves.size(); i++)
			{
				if(slaves[i]->getrank() == tmprank)
				{
					delete slaves[i];
					slaves.erase (slaves.begin() + i);
					break;
				}
			}
			continue;
		}
		else if (readbytes < 0)
		{
			continue;
		}
		else
		{
			if (strncmp (read_buf, "peerids", 7) == 0)
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
				log->info ("A task completed [jobid: %d][CompletedTasks: %d] [NumTasks: %d ]",
					ajobid, thejob->get_numcompleted_tasks(), thejob->get_numtasks());
			}
			else
			{
				log->info ("Undefined message from slave node: %s ", read_buf);
				log->info ("Undefined message size: %s", readbytes);
			}
		}

		// break if all slaves and clients are closed
		if (slaves.size() == 0 && clients.size() == 0)
		{
			break;
		}
	}
	log->info ("mpi_master_listener end");
	exit(EXIT_SUCCESS);
}

void socket_listener (int args)
{
	int serverfd = args;
	int readbytes = 0;
	int tmpfd = -1; // store fd of new connected node temporarily
	int elapsed = 0; // time elapsed im msec
	char* haddrp;
	struct sockaddr_in connaddr;
	int addrlen = sizeof (connaddr);
	struct timeval time_start, time_end;

	gettimeofday (&time_start, NULL);
	gettimeofday (&time_end, NULL);

	// listen signals from nodes and listen to node connection
	while (1)
	{
		// check client (or job) connection
		tmpfd = accept (serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		
		// it should be changed with mpi

		if (tmpfd >= 0)
		{
			// blocking read to check identification of connected node
			memset (read_buf, 0, BUF_SIZE);
			readbytes = nbread (tmpfd, read_buf);

			if (readbytes == 0)
			{
				log->info ("Connection closed from client before getting request");
				close (tmpfd);
				tmpfd = -1;
				continue;
			}

			fcntl (tmpfd, F_SETFL, O_NONBLOCK);   // set socket to be non-blocking socket to avoid deadlock

			if (strncmp (read_buf, "client", 6) == 0)       // if connected node is a client
			{
				// get ip address of client
				haddrp = inet_ntoa (connaddr.sin_addr);
				log->info  ("Client node connected from %s \n", haddrp);
				clients.push_back (new connclient (tmpfd));
			}
			else if (strncmp (read_buf, "job", 3) == 0)         // if connected node is a job
			{
				// limit the maximum available job connection
				if (jobs.size() == (unsigned) max_job)
				{
					log->info ("Cannot accept any more job request due to maximum job connection limit");
					log->info ("\tClosing connection from the job...");
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "nospace");
					nbwrite (tmpfd, write_buf);
					close (tmpfd);
					break;
				}

				// send the job id to the job
				stringstream jobidss;
				jobidss << "jobid ";
				jobidss << jobidclock;
				memset (write_buf, 0, BUF_SIZE);
				strcpy (write_buf, jobidss.str().c_str());
				nbwrite (tmpfd, write_buf);
				log->info ("Job %i arrived", jobidclock);
				jobs.push_back (new master_job (jobidclock, tmpfd));
				jobidclock++;
			}
			else
			{
				log->info ("Unidentified connected node: ");
				log->info ("Closing connection to the node...");
				close (tmpfd);
			}
		}

		// listen to clients
		for (int i = 0; (unsigned) i < clients.size(); i++)
		{
			readbytes = nbread (clients[i]->getfd(), read_buf);

			if (readbytes == 0)     // connection closed from client
			{
				log->info ("Connection from a client closed");
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
				log->info ("Message accepted from client: %s ", read_buf);

				if (strncmp (read_buf, "stop", 4) == 0)       // "stop" signal arrived
				{
					int currfd = clients[i]->getfd(); // store the current client's fd

					// stop all slave
					for (int j = 0; (unsigned) j < slaves.size(); j++)
					{
						memset (write_buf, 0, BUF_SIZE);
						strcpy (write_buf, "close");
						MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, slaves[j]->getrank(), 0, MPI_COMM_WORLD, request);
						// blocking read from slave
						MPI_Recv(&read_buf, BUF_SIZE, MPI_CHAR, slaves[j]->getrank(), 0, MPI_COMM_WORLD, &status);
						MPI_Get_count(&status, MPI_CHAR, &readbytes);

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
							MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, slaves[j]->getrank(), 0, MPI_COMM_WORLD, request);
							j--;
							continue;
						}

						log->info ("Connection from a slave closed");
					}

					log->info ("All slaves closed");

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

						log->info ("Connection from a client closed");
					}

					log->info ("All clients closed");
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
					log->info ("Undefined signal from client: %s ", read_buf);
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
				log->info ("Job terminated abnormally");
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
					log->info ("Job %d successfully completed", jobs[i]->getjobid());
					// clear up the completed job
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "terminate");
					nbwrite (jobs[i]->getjobfd(), write_buf);
					// delete the job from the vector jobs
					delete jobs[i];
					jobs.erase (jobs.begin() + i);
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
								log->info ("Debugging: the 'jobconf' protocol conflicts.");
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
							log->info ("Unknown job configure message from job: %s", token);
						}

						// process next configure
						token = strtok (NULL, " ");
					}

					if (jobs[i]->getnummap() == 0)
					{
						jobs[i]->setnummap (jobs[i]->get_numinputpaths());
					}

					// create map tasks
					for (int j = 0; j < jobs[i]->getnummap(); j++)
					{
						jobs[i]->add_task (new master_task (jobs[i], MAP));
					}

					// map inputpaths to each map tasks
					int path_iteration = 0;

					while (path_iteration < jobs[i]->get_numinputpaths())
					{
						for (int j = 0; j < jobs[i]->get_numtasks() && path_iteration < jobs[i]->get_numinputpaths(); j++)
						{
							jobs[i]->get_task (j)->add_inputpath (jobs[i]->get_inputpath (path_iteration));
							path_iteration++;
						}
					}

					// set job stage as MAP_STAGE
					jobs[i]->set_stage (MAP_STAGE);
				}
				else     // undefined signal
				{
					log->info ("Undefined signal from job: %s", read_buf);
				}
			}

			// check if all task finished
			if (jobs[i]->get_numtasks() == jobs[i]->get_numcompleted_tasks())
			{
				if (jobs[i]->get_stage() == MAP_STAGE)     // if map stage is finished
				{
					if (jobs[i]->status == TASK_FINISHED)     // information of numblock is gathered
					{
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
						//nbwrite (ipcfd, write_buf);
						handle_iwritefinish (write_buf);
						jobs[i]->status = REQUEST_SENT;
					}
					else if (jobs[i]->status == REQUEST_SENT)
					{
						// do nothing(just wait for the respond)
					}
					else     // status == RESPOND_RECEIVED
					{
						// determine the number of reducers
						if (jobs[i]->getnumreduce() <= 0)
						{
							jobs[i]->setnumreduce (jobs[i]->peerids.size());
						}
						else if ( (unsigned) jobs[i]->getnumreduce() > jobs[i]->peerids.size())
						{
							// TODO: enable much more number of reducers
							jobs[i]->setnumreduce (jobs[i]->peerids.size());
						}

						// generate reduce tasks and feed each reducer dedicated peer with numblock information
						for (int j = 0; j < jobs[i]->getnumreduce(); j++)
						{
							master_task* newtask = new master_task (jobs[i], REDUCE);
							jobs[i]->add_task (newtask);
						}

						int index = 0;
						int iteration = 0;

						// while all peerids are given to reducers
						for (set<int>::iterator it = jobs[i]->peerids.begin(); it != jobs[i]->peerids.end(); it++)
						{
							jobs[i]->get_waitingtask (iteration)->numiblocks.push_back (jobs[i]->numiblocks[index]);
							jobs[i]->get_waitingtask (iteration)->peerids.push_back (*it);
							index++;
							iteration++;

							if (iteration >= jobs[i]->getnumreduce())
							{
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
					log->info ("Job %d completed successfully", jobs[i]->getjobid());
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
		check_iwriterequest (read_buf);
		check_cache_slaves ();

		if (strncmp (read_buf, "numblocks", 9) == 0) 
		{
			char* token;
			int jobid;
			token = strtok (read_buf, " ");   // token: "numblocks"
			token = strtok (NULL, " ");   // token: jobid
			jobid = atoi (token);
			token = strtok (NULL, " ");   // first number of block

			for (int i = 0; jobs.size(); i++)
			{
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

		// process and schedule jobs and tasks
		// EMKDE scheduler: schedule the task where the input is located
		for (int i = 0; (unsigned) i < jobs.size(); i++)
		{
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

					/*
					   if(slaves[nodeindex]->getnumrunningtasks() >= slaves[nodeindex]->getmaxmaptask()) // choose alternative slot
					   continue;
					 */
					if (slaves[nodeindex]->getnumrunningtasks() >= slaves[nodeindex]->getmaxmaptask())     // choose alternative slot
					{
						nodeindex = -1;

						for (int h = 0; (unsigned) h < slaves.size(); h++)
						{
							if (slaves[h]->getnumrunningtasks() >= slaves[h]->getmaxmaptask())
							{
								continue;
							}
							else
							{
								nodeindex = h;
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
					MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, slaves[nodeindex]->getrank(), 0, MPI_COMM_WORLD);
					// prepare inputpath message
					int iter = 0;
					message = "inputpath";

					while (iter < thetask->get_numinputpaths())
					{
						if (message.length() + thetask->get_inputpath (iter).length() + 1 < BUF_SIZE)
						{
							message.append (" ");
							message.append (thetask->get_inputpath (iter));
						}
						else
						{
							if (thetask->get_inputpath (iter).length() + 10 > BUF_SIZE)
							{
								log->info ("The length of inputpath exceeded the limit");
							}

							// send message to slave
							memset (write_buf, 0, BUF_SIZE);
							strcpy (write_buf, message.c_str());
							MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, slaves[nodeindex]->getrank(), 0, MPI_COMM_WORLD);
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
						MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, slaves[nodeindex]->getrank(), 0, MPI_COMM_WORLD);
					}

					// notify end of inputpaths
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, "Einput");
					MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, slaves[nodeindex]->getrank(), 0, MPI_COMM_WORLD);
					// forward waiting task to slave slot
					jobs[i]->schedule_task (thetask, slaves[nodeindex]);
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
					MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, slaves[nodeindex]->getrank(), 0, MPI_COMM_WORLD);
					message = "inputpath";

					for (int j = 0; (unsigned) j < thetask->peerids.size(); j++)       // don't need to worry about BUF_SIZE overflow in reducer case
					{
						stringstream ss;
						ss << " ";
						ss << thetask->peerids[j];
						ss << " ";
						ss << thetask->numiblocks[j];
						message.append (ss.str());
					}

					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					MPI_Send(&write_buf, BUF_SIZE, MPI_CHAR, slaves[nodeindex]->getrank(), 0, MPI_COMM_WORLD);
					// forward waiting task to slave slot
					jobs[i]->schedule_task (thetask, slaves[nodeindex]);
				}

				continue;
			}
		}

		gettimeofday (&time_end, NULL);
		elapsed += 1000 * (time_end.tv_sec - time_start.tv_sec);
		elapsed += (time_end.tv_usec - time_start.tv_usec) / 1000;

		if (elapsed > UPDATEINTERVAL)     // UPDATE INTERVAL from EM-KDE
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

			//nbwrite (ipcfd, write_buf);
			handle_boundaries(write_buf);
			gettimeofday (&time_start, NULL);
			elapsed = 0;
		}

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
	log->info ("Master server closed");
	log->info ("Exiting master...");
	Logger::disconnect (log);
	exit(EXIT_SUCCESS);
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

void handle_boundaries(char * read_buf)
{
	for (int i = 0; (unsigned) i < cache_clients.size(); i++)
	{
		MPI_Send(&read_buf, BUF_SIZE, MPI_CHAR, cache_clients[i]->getrank(), 0, MPI_COMM_WORLD);
	}
}

void handle_iwritefinish(char * read_buf)
{
	char tmp_buf[BUF_SIZE];
	string message;
	stringstream ss;
	char* token;
	int jobid;
	token = strtok (read_buf, " ");   // token <- "iwritefinish"
	token = strtok (NULL, " ");   // jobid
	jobid = atoi (token);
	// add the request to the vector iwfrequests
	iwfrequest* therequest = new iwfrequest (jobid);
	iwfrequests.push_back (therequest);
	// prepare message for each client
	ss << "iwritefinish ";
	ss << jobid;
	message = ss.str();
	memset (tmp_buf, 0, BUF_SIZE);
	strcpy (tmp_buf, message.c_str());
	token = strtok (NULL, " ");
	int peerid;

	// request to the each peer right after tokenize each peer id
	while (token != NULL)
	{
		peerid = atoi (token);
		therequest->add_request (peerid);
		// send message to target client
		MPI_Send(&tmp_buf, BUF_SIZE, MPI_CHAR, cache_clients[peerid]->getrank(), 0, MPI_COMM_WORLD);
		// tokenize next peer id
		token = strtok (NULL, " ");
	}
}

// check whether the request has received all responds
void check_iwriterequest(char * tmp_buf)
{
	for (int i = 0; (unsigned) i < iwfrequests.size(); i++)
	{
		if (iwfrequests[i]->is_finished()) /* send numblock information in order to the master */ {
			bzero (tmp_buf, BUF_SIZE);
			stringstream ss;

			ss << "numblocks " << iwfrequests[i]->get_jobid();

			for (int j = 0; (unsigned) j < iwfrequests[i]->peerids.size(); j++)
			{
				ss << " " << iwfrequests[i]->numblocks[j];
			}

			ss.str().copy(tmp_buf, BUF_SIZE);
			// clear the iwfrequest
			delete iwfrequests[i];
			iwfrequests.erase (iwfrequests.begin() + i);
			i--;
		}
	}
}

void check_cache_slaves ()
{
	char tmp_buf [BUF_SIZE];
	for (int i = 0; (unsigned) i < cache_clients.size(); i++)
	{
		// do nothing currently
		MPI_Recv(&tmp_buf, BUF_SIZE, MPI_CHAR, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
		if (readbytes > 0)
		{
			if (strncmp (tmp_buf, "iwritefinish", 12) == 0)
			{
				char* token;
				int jobid;
				int numblock;
				token = strtok (tmp_buf, " ");   // token <- "iwritefinish"
				token = strtok (NULL, " ");   // token <- jobid
				jobid = atoi (token);
				token = strtok (NULL, " ");   // token <- numblock
				numblock = atoi (token);

				for (int j = 0; (unsigned) j < iwfrequests.size(); j++)
				{
					if (iwfrequests[j]->get_jobid() == jobid)
					{
						iwfrequests[j]->add_receive (i, numblock);
						break;
					}
				}
			}
			else
			{
				log->info ("[cacheserver]Abnormal message from clients");
			}

		}
		else if (readbytes == 0)
		{
			log->info ("[cacheserver]Connection to clients abnormally closed");
			usleep (100000);
		}
	}
}
