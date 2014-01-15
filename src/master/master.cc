#include "master.hh"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <mapreduce/definitions.hh>
#include "master_job.hh"
#include "master_task.hh"
#include "connslave.hh"
#include "connclient.hh"

using namespace std;

vector<connslave*> slaves;
vector<connclient*> clients;

vector<master_job*> jobs;


int num_slave = -1;
int backlog = -1;
int port = -1;
int max_job = -1;
int jobidclock = 0; // job id starts 0

char read_buf[BUF_SIZE]; // read buffer for signal_listener thread
char write_buf[BUF_SIZE]; // write buffer for signal_listener thread

// --------------------------master protocol---------------------------------
// 1. whoareyou: send a message to identify the connected node
// 2. close: let the destination node close the connection from the master
// 3. ignored: let connected node know message from it was ignored
// 4. result: contains resulting messeage to client followed by 'result:'
// --------------------------------------------------------------------------
// TODO: make protocols to integer or enum

int main(int argc, char** argv)
{
	// initialize data structures from setup.conf
	ifstream conf;
	string token;
	string confpath = LIB_PATH;
	confpath.append("setup.conf");
	conf.open(confpath.c_str());

	conf>>token;
	while(!conf.eof())
	{
		if(token == "backlog")
		{
			conf>>token;
			backlog = atoi(token.c_str());
		}
		else if(token == "port")
		{
			conf>>token;
			port = atoi(token.c_str());
		}
		else if(token == "max_job")
		{
			conf>>token;
			max_job = atoi(token.c_str());
		}
		else if(token == "num_slave")
		{
			conf>>token;
			num_slave = atoi(token.c_str());
		}
		else if(token == "master_address")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else
		{
			cout<<"[master]Unknown configure record: "<<token<<endl;
		}
		conf>>token;
	}
	conf.close();
	// verify initialization
	if(backlog == -1)
	{
		cout<<"[master]backlog should be specified in the setup.conf"<<endl;
		exit(1);
	}
	if(port == -1)
	{
		cout<<"[master]port should be specified in the setup.conf"<<endl;
		exit(1);
	}
	if(max_job == -1)
	{
		cout<<"[master]max_job should be specified in the setup.conf"<<endl;
		exit(1);
	}
	if(num_slave == -1)
	{
		cout<<"[master]num_slave should be specified in the setup.conf"<<endl;
		exit(1);
	}

	int serverfd = open_server(port);
	if(serverfd < 0)
	{
		cout<<"[master]Openning server failed"<<endl;
		return 1;
	}

	struct sockaddr_in connaddr;
	int addrlen = sizeof(connaddr);
	char* haddrp;

	while(1) // receives connection from slaves
	{
		int fd;
		fd = accept(serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		if(fd < 0)
		{
			cout<<"[master]Accepting failed"<<endl;

			// sleep 0.0001 seconds. change this if necessary
			usleep(100);
			continue;
		}
		else
		{
			// check if the accepted node is slave or client
			write(fd, "whoareyou", BUF_SIZE);
			read(fd, read_buf, BUF_SIZE);
			if(strncmp(read_buf, "slave", 5) == 0) // slave connected
			{
				// add connected slave to the slaves
				slaves.push_back(new connslave(fd));

				// get ip address of slave
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("slave node connected from %s \n", haddrp);
			}
			else if(strncmp(read_buf, "client", 6) == 0) // client connected
			{
				clients.push_back(new connclient(fd));

				// set sockets to be non-blocking socket to avoid deadlock
				fcntl(clients.back()->getfd(), F_SETFL, O_NONBLOCK);

				// get ip address of client
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("a client node connected from %s \n", haddrp);
			}
			else if(strncmp(read_buf, "job", 3) == 0)
			{
				// TODO: deal with the case that a job joined the
				// server before all slave connected
			}
			else // unknown connection
			{
				// TODO: deal with this case
				cout<<"[master]Unknown connection"<<endl;
			}

			// break if all slaves are connected
			if(slaves.size() == (unsigned)num_slave)
			{
				cout<<"[master]All slave nodes are connected successfully"<<endl;
				// set maximum number of task 4 for each slave as default
				for(int i=0;(unsigned)i<slaves.size();i++)
					slaves[i]->setmaxtask(4);

				break;
			}
			else if(slaves.size() > (unsigned)num_slave)
			{
				cout<<"[master]Number of slave connection exceeded allowed limits"<<endl;
				cout<<"[master]\tDebugging needed on this problem"<<endl;
			}
			// sleeps for 0.0001 seconds. change this if necessary
			usleep(100);
		}
	}

	// set sockets to be non-blocking socket to avoid deadlock
	fcntl(serverfd, F_SETFL, O_NONBLOCK);
	for(int i=0;(unsigned)i<slaves.size();i++)
		fcntl(slaves[i]->getfd(), F_SETFL, O_NONBLOCK);

	// create listener thread and run
	pthread_t listener_thread;
	pthread_create(&listener_thread, NULL, signal_listener, (void*)&serverfd);

	// sleeping loop which prevents process termination
	while(1)
		sleep(1);
	return 0;
}

int open_server(int port)
{
	int serverfd;
	struct sockaddr_in serveraddr;

	// socket open
	serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if(serverfd<0)
		cout<<"[master]Socket opening failed"<<endl;

	// bind
	memset((void *) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) port);
	if(bind(serverfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		cout<<"[master]Binding failed"<<endl;
		return -1;
	}

	// listen
	if(listen(serverfd, backlog) < 0)
	{
		cout<<"[master]Listening failed"<<endl;
		return -1;
	}
	return serverfd;
}

void* signal_listener(void* args)
{
	int serverfd = *((int*)args);
	int readbytes = 0;
	int tmpfd = -1; // store fd of new connected node temporarily
	char* haddrp;
	struct sockaddr_in connaddr;
	int addrlen = sizeof(connaddr);

	// listen signals from nodes and listen to node connection
	while(1)
	{
		// check client connection
		tmpfd = accept(serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		if(tmpfd >= 0)
		{
			fcntl(tmpfd, F_SETFL, O_NONBLOCK); // set socket to be non-blocking socket to avoid deadlock

			// send "whoareyou" message to connected node
			write(tmpfd, "whoareyou", BUF_SIZE);

			// blocking read to check identification of connected node
			while(1) // break after reply received or connection abnormally closed
			{
				memset(read_buf, 0, BUF_SIZE);
				readbytes = read(tmpfd, read_buf, BUF_SIZE);
				if(readbytes == 0)
				{
					cout<<"[master]Connection closed from client before getting request"<<endl;

					close(tmpfd);
					tmpfd = -1;
					break;
				}
				else if(readbytes < 0)
				{
					// sleep for 0.0001 second. change this if necessary
					usleep(100);
					continue;
				}
				else // reply arrived
				{
					break;
				}
				// sleep for 0.0001 second.  change this if necessdary
				usleep(100);
			}
			if(strncmp(read_buf, "client", 6) == 0) // if connected node is a client
			{
				// get ip address of client
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("[master]Client node connected from %s \n", haddrp);

				clients.push_back(new connclient(tmpfd));
			}
			else if(strncmp(read_buf, "slave", 5) == 0)
			{
				cout<<"[master]Unexpected connection from slave: "<<endl;
				cout<<"[master]Closing connection to the slave..."<<endl;
				// check this code
				write(tmpfd, "close", BUF_SIZE);
				close(tmpfd);
			}
			else if(strncmp(read_buf, "job", 3) == 0) // if connected node is a job
			{
				// limit the maximum available job connection
				if(jobs.size() == (unsigned)max_job)
				{
					cout<<"[master]Cannot accept any more job request due to maximum job connection limit"<<endl;
					cout<<"[master]\tClosing connection from the job..."<<endl;
					write(tmpfd, "nospace", BUF_SIZE);
					close(tmpfd);
					break;
				}

				// send the job id to the job
				stringstream jobidss;
				jobidss<<"jobid ";
				jobidss<<jobidclock;
				strcpy(read_buf, jobidss.str().c_str());
				write(tmpfd, read_buf, BUF_SIZE);

				cout<<"[master]Job "<<jobidclock<<" arrived"<<endl;
				jobs.push_back(new master_job(jobidclock, tmpfd));
				jobidclock++;
			}
			else
			{
				cout<<"[master]Unidentified connected node: "<<endl;
				cout<<"[master]Closing connection to the node..."<<endl;
				close(tmpfd);
			}
		}

		// listen to slaves
		for(int i=0; (unsigned)i<slaves.size(); i++)
		{
			memset(read_buf, 0, BUF_SIZE);
			readbytes = read(slaves[i]->getfd(), read_buf, BUF_SIZE);
			if(readbytes == 0) // connection closed from slave
			{
				cout<<"[master]Connection from a slave closed"<<endl;
				delete slaves[i];
				slaves.erase(slaves.begin()+i);
				i--;
				continue;
			}
			else if(readbytes < 0)
			{
				continue;
			}
			else // signal from the slave
			{
				if(strncmp(read_buf, "key", 3) == 0) // key signal arrived
				{
					char* token;
					master_job* thejob;
					token = strtok(read_buf, " "); // token <- "key"
					token = strtok(NULL, " "); // token <- "jobid"
					token = strtok(NULL, " "); // token <- job id
					
					thejob = find_jobfromid(atoi(token));

					token = strtok(NULL, " "); // token <- key value
					thejob->add_key(token); // token 
				}
				else if(strncmp(read_buf, "taskcomplete", 12) == 0) // "taskcomplete" signal arrived
				{
					char* token;
					int ajobid, ataskid;
					master_job* thejob;
					master_task* thetask;

					token = strtok(read_buf, " "); // token <- "taskcomplete"
					token = strtok(NULL, " "); // token <- "jobid"
					token = strtok(NULL, " "); // token <- job id
					ajobid = atoi(token);

					token = strtok(NULL, " "); // token <- "taskid"
					token = strtok(NULL, " "); // token <- task id
					ataskid = atoi(token);

					thejob = find_jobfromid(ajobid);
					thetask = thejob->find_taskfromid(ataskid);
					thejob->finish_task(thetask, slaves[i]);
				}
				else
				{
					cout<<"[master]Undefined message from slave node: "<<read_buf<<endl;
				}
			}
		}

		// listen to clients
		for(int i=0; (unsigned)i<clients.size(); i++)
		{
			memset(read_buf, 0, BUF_SIZE);
			readbytes = read(clients[i]->getfd(), read_buf, BUF_SIZE);
			if(readbytes == 0) // connection closed from client
			{
				cout<<"[master]Connection from a client closed"<<endl;
				delete clients[i];
				clients.erase(clients.begin()+i);
				i--;
				continue;
			}
			else if(readbytes < 0)
			{
				continue;
			}
			else // signal from the client
			{
				cout<<"[master]Message accepted from client: "<<read_buf<<endl;
				if(strncmp(read_buf, "stop", 4) == 0) // "stop" signal arrived
				{
					int currfd = clients[i]->getfd(); // store the current client's fd

					// stop all slave
					for(int j=0;(unsigned)j<slaves.size();j++)
					{
						write(slaves[j]->getfd(), "close", BUF_SIZE);

						// blocking read from slave
						while(true) // until the slave is closed
						{
							readbytes = read(slaves[j]->getfd(), read_buf, BUF_SIZE);
							if(readbytes == 0) // closing slave succeeded
							{
								delete slaves[j];
								slaves.erase(slaves.begin()+j);
								j--;
								break;
							}
							else if(readbytes < 0)
							{
								// sleeps for 0.0001 seconds. change this if necessary
								usleep(100);
								continue;
							}
							else // message arrived before closed
							{
								write(slaves[j]->getfd(),"ignored",BUF_SIZE);
							}
						}
						cout<<"[master]Connection from a slave closed"<<endl;
					}
					cout<<"[master]All slaves closed"<<endl;

					// stop all client except the one requested stop
					for(int j=0;(unsigned)j<clients.size();j++)
					{
						if(currfd==clients[j]->getfd()) // except the client who requested the stop
							continue;

						write(clients[j]->getfd(), "close", BUF_SIZE);

						// blocking read from client
						while(true) // until the client is closed
						{
							readbytes = read(clients[j]->getfd(), read_buf, BUF_SIZE);
							if(readbytes == 0) // closing client succeeded
							{
								delete clients[j];
								clients.erase(clients.begin()+j);
								j--;
								break;
							}
							else if(readbytes < 0)
							{
								// sleeps for 0.0001 seconds. change this if necessary
								usleep(100);
								continue;
							}
							else // message arrived before closed
							{
								write(clients[j]->getfd(),"ignored",BUF_SIZE);
							}
						}
					}
					cout<<"[master]All clients closed"<<endl;

					write(clients[i]->getfd(), "result: stopping successful", BUF_SIZE);
				}
				else if(strncmp(read_buf, "numslave", 8) == 0) // "numslave" signal arrived
				{
					string ostring = "result: number of slave nodes = ";
					stringstream ss;
					ss<<slaves.size();
					ostring.append(ss.str());
					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, ostring.c_str());
					write(clients[i]->getfd(), write_buf, BUF_SIZE);
				}
				else if(strncmp(read_buf, "numclient", 9) == 0) // "numclient" signal arrived
				{
					string ostring = "result: number of client nodes = ";
					stringstream ss;
					ss<<clients.size();
					ostring.append(ss.str());
					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, ostring.c_str());
					write(clients[i]->getfd(), write_buf, BUF_SIZE);
				}
				else if(strncmp(read_buf, "numjob", 6) == 0) // "numjob" signal arrived
				{
					string ostring = "result: number of running jobs = ";
					stringstream ss;
					ss<<jobs.size();
					ostring.append(ss.str());
					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, ostring.c_str());
					write(clients[i]->getfd(), write_buf, BUF_SIZE);
				}
				else if(strncmp(read_buf, "numtask", 7) == 0) // "numtask" signal arrived
				{
					string ostring = "result: number of running tasks = ";
					stringstream ss;
					int numtasks = 0;
					for(int j=0;(unsigned)j<jobs.size();j++)
					{
						numtasks += jobs[j]->get_numtasks();
					}
					ss<<numtasks;
					ostring.append(ss.str());
					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, ostring.c_str());
					write(clients[i]->getfd(), write_buf, BUF_SIZE);
				}
				else // undefined signal
				{
					cout<<"[master]Undefined signal from client: "<<read_buf<<endl;
					write(clients[i]->getfd(), "result: error. the request is unknown", BUF_SIZE);
				}
			}
		}

		// check messages from jobs
		for(int i=0; (unsigned)i<jobs.size(); i++)
		{
			memset(read_buf, 0, BUF_SIZE);
			readbytes = read(jobs[i]->getjobfd(), read_buf, BUF_SIZE);
			if(readbytes == 0) // connection to the job closed. maybe process terminated
			{
				delete jobs[i];
				jobs.erase(jobs.begin()+i);
				i--;
				cout<<"[master]Job terminated abnormally"<<endl;
			}
			else if(readbytes > 0) // signal from the job
			{
				if(strncmp(read_buf, "complete", 8) == 0) // "succompletion" signal arrived
				{
					cout<<"[master]Job "<<jobs[i]->getjobid()<<" successfully completed"<<endl;

					// clear up the completed job
					write(jobs[i]->getjobfd(), "terminate", BUF_SIZE);

					// delete the job from the vector jobs
					delete jobs[i];
					jobs.erase(jobs.begin()+i);
					i--;
				}
				else if(strncmp(read_buf, "jobconf", 7) == 0) // "jobconf" message arrived
				{
					char* token;
					token = strtok(read_buf, " "); // token -> jobconf
					token = strtok(NULL, " "); // token -> nummap expected

					// parse all configure
					while(token != NULL)
					{
						if(strncmp(token, "argcount", 8) == 0)
						{
							// NOTE: there should be at leat 1 arguments(program path name)
							token = strtok(NULL, " "); // token <- argument count
							jobs[i]->setargcount(atoi(token));

							// process next configure
							token = strtok(NULL, " "); // token -> argvalues

							// check the protocl
							if(strncmp(token, "argvalues", 9) != 0) // if the token is not 'argvalues'
								cout<<"Debugging: the 'jobconf' protocol conflicts."<<endl;

							char** arguments = new char*[jobs[i]->getargcount()];
							for(int j=0;j<jobs[i]->getargcount();j++)
							{
								token = strtok(NULL, " ");
								arguments[j] = new char[strlen(token)+1];
								strcpy(arguments[j], token);
							}
							jobs[i]->setargvalues(arguments);
						}
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
						else
						{
							cout<<token<<": unknown job configure in the master side"<<endl;
						}

						// process next configure
						token = strtok(NULL, " ");
					}
					if(jobs[i]->getnummap() == 0)
						jobs[i]->setnummap(jobs[i]->get_numinputpaths());

					// create map tasks
					for(int j=0;j<jobs[i]->getnummap();j++)
					{
						jobs[i]->add_task(new master_task(jobs[i], MAP));
					}

					// map inputpaths to each map tasks
					int path_iteration = 0;
					while(path_iteration<jobs[i]->get_numinputpaths())
					{
						for(int j=0;j<jobs[i]->get_numtasks() && path_iteration<jobs[i]->get_numinputpaths();j++)
						{
							jobs[i]->get_task(j)->add_inputpath(jobs[i]->get_inputpath(path_iteration));
							path_iteration++;
						}
					}

					// set job status as MAP_STAGE
					jobs[i]->set_stage(MAP_STAGE);
				}
				else // undefined signal
				{
					cout<<"[master]Undefined signal from job: "<<read_buf<<endl;
				}
			}

			// check if all task finished
			if(jobs[i]->get_numtasks() == jobs[i]->get_numcompleted_tasks())
			{
				if(jobs[i]->get_stage() == MAP_STAGE) // if map stage is finished
				{
					// send message to the job to inform that map phase is completed
					write(jobs[i]->getjobfd(), "mapcomplete", BUF_SIZE);

					cout<<"[master]Number of keys generated from map phase: "<<jobs[i]->get_numkey()<<endl;
					// fork reduce tasks
					for(set<string>::iterator it = jobs[i]->get_keybegin();it != jobs[i]->get_keyend(); it++)
					{
						master_task* newtask = new master_task(jobs[i], REDUCE);
						jobs[i]->add_task(newtask);
						newtask->add_inputpath(*it);
					}
					jobs[i]->set_stage(REDUCE_STAGE);
				}
				else if(jobs[i]->get_stage() == REDUCE_STAGE) // if reduce stage is finished
				{
					// send message to the job to complete the job
					write(jobs[i]->getjobfd(), "complete", BUF_SIZE);
					cout<<"[master]Job "<<jobs[i]->getjobid()<<" completed successfully"<<endl;

					jobs[i]->set_stage(COMPLETED_STAGE);
					// clear the job from the vector and finish
					delete jobs[i];
					jobs.erase(jobs.begin()+i);
					i--;

					continue;
				}
				else
				{
					// pass the case for INITIAL_STAGE and COMPLETED_STAGE
				}
			}
		}

		// process and schedule jobs and tasks
		for(int i=0; (unsigned)i<jobs.size(); i++)
		{
			// default scheduler: FCFS-like scheduler
			for(int j=0;(unsigned)j<slaves.size();j++)
			{
				while((slaves[j]->getnumrunningtasks() < slaves[j]->getmaxtask()) && (jobs[i]->get_lastwaitingtask() != NULL)) // schedule all the slot if none of the slot is avilable
				{
					master_task* thetask = jobs[i]->get_lastwaitingtask();
					// write to the slave the task information
					stringstream ss;
					ss<<"tasksubmit ";
					ss<<"jobid ";
					ss<<jobs[i]->getjobid();
					ss<<" ";
					ss<<"taskid ";
					ss<<thetask->gettaskid();
					ss<<" role ";
					if(thetask->get_taskrole() == MAP)
						ss<<"MAP";
					else if(thetask->get_taskrole() == REDUCE)
						ss<<"REDUCE";
else
cout<<"[master]Debugging: the role of the task not defined in the initialization step:650";

					ss<<" inputpath ";
					ss<<thetask->get_numinputpaths(); // number of input paths

					// parse all the input paths
					for(int k=0;k<thetask->get_numinputpaths();k++)
					{
						ss<<" ";
						ss<<thetask->get_inputpath(k);
					}

					ss<<" argcount ";
					ss<<thetask->get_job()->getargcount();

					// NOTE: there should be at leat 1 arguments(program path name)
					ss<<" argvalues";
					for(int k=0;k<thetask->get_job()->getargcount();k++)
					{
						ss<<" ";
						ss<<thetask->get_job()->getargvalue(k);
					}
if(ss.str().length()>=BUF_SIZE)
cout<<"[master]Debugging: the argument string exceeds the limited length"<<endl;
					strcpy(write_buf, ss.str().c_str());
					write(slaves[j]->getfd(), write_buf, BUF_SIZE);

					// forward waiting task to slave slot
					jobs[i]->schedule_task(thetask, slaves[j]);
					// sleeps for 0.0001 seconds. change this if necessary
					usleep(100);
				}
			}
		}

		// break if all slaves and clients are closed
		if(slaves.size() == 0 && clients.size() == 0)
			break;

		// sleeps for 0.0001 seconds. change this if necessary
		usleep(100);
	}

	// close master socket
	close(serverfd);
	cout<<"[master]Master server closed"<<endl;

	cout<<"[master]Exiting master..."<<endl;

	exit(0);
}

master_job* find_jobfromid(int id)
{
	for(int i=0;(unsigned)i<jobs.size();i++)
	{
		if(jobs[i]->getjobid() == id)
			return jobs[i];
	}
cout<<"[master]No such a job with input job id in find_jobfromid() function."<<endl;
	return NULL;
}
