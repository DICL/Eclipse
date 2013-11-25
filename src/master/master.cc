#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "master.hh"
#include "connslave.hh"
#include "connclient.hh"
#include <mapreduce/definitions.hh>
#include <mapreduce/job.hh>
#include <mapreduce/task.hh>

using namespace std;

vector<connslave*> slaves;
vector<connclient*> clients;

vector<job*> jobs;
vector<task*> waiting_tasks;
vector<task*> running_tasks;

int num_slave = -1;
int backlog = -1;
int port = -1;
int max_client = -1;

char read_buf_signal[BUF_SIZE]; // read buffer for signal_listener thread
char read_buf_client[BUF_SIZE]; // read buffer for accept_client thread
char write_buf_signal[BUF_SIZE]; // write buffer for signal_listener thread
char write_buf_client[BUF_SIZE]; // write buffer for accept_client thread

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

	while(1)
	{
		conf>>token;
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
		else if(token == "max_client")
		{
			conf>>token;
			max_client = atoi(token.c_str());
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
		else if(token == "end")
		{
			break;
		}
		else
		{
			cout<<"[master]Unknown configure record: "<<token<<endl;
		}
	}
	conf.close();
	// verify initialization
	if(backlog == -1)
	{
		cout<<"[master]backlog should be specified in the setup.conf"<<endl;
		return 1;
	}
	if(port == -1)
	{
		cout<<"[master]port should be specified in the setup.conf"<<endl;
		return 1;
	}
	if(max_client == -1)
	{
		cout<<"[master]max_client should be specified in the setup.conf"<<endl;
		return 1;
	}
	if(num_slave == -1)
	{
		cout<<"[master]num_slave should be specified in the setup.conf"<<endl;
		return 1;
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
			continue;
		}
		else
		{
			// check if the accepted node is slave or client
			write(fd, "whoareyou", BUF_SIZE);
			read(fd, read_buf_client, BUF_SIZE);
			if(strncmp(read_buf_client, "slave", 5) == 0) // slave connected
			{
				// add connected slave to the slaves
				slaves.push_back(new connslave(fd));

				// get ip address of slave
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("slave node connected from %s \n", haddrp);
			}
			else if(strncmp(read_buf_client, "client", 6) == 0) // client connected
			{
				//  limit the maximum available client connection
				if(clients.size() == max_client)
				{
					cout<<"[master]Cannot accept any more client request due to maximum client connection limit"<<endl;
					cout<<"[master]\tClosing connection from the client..."<<endl;
					write(fd, "nospace", 7);

					close(fd);
					continue; // continue the while loop
				}

				clients.push_back(new connclient(fd));

				// set sockets to be non-blocking socket to avoid deadlock
				fcntl(clients.back()->getfd(), F_SETFL, O_NONBLOCK);

				// get ip address of client
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("client node connected from %s \n", haddrp);
			}
			else // unknown connection
			{
				// TODO: deal with this case
				cout<<"[master]Unknown connection"<<endl;
			}

			if(slaves.size() == num_slave)
			{
				cout<<"[master]All slave nodes are connected successfully"<<endl;
				break;
			}
			else if(slaves.size() > num_slave)
				cout<<"[master]Number of slave connection exceeded allowed limits"<<endl;
		}
		// sleeps for 0.01 seconds. change this if necessary
		usleep(10000);
	}

	// set sockets to be non-blocking socket to avoid deadlock
	fcntl(serverfd, F_SETFL, O_NONBLOCK);
	for(int i=0;i<slaves.size();i++)
		fcntl(slaves[i]->getfd(), F_SETFL, O_NONBLOCK);

	// create listener thread
	pthread_t listener_thread;
	pthread_t client_acceptor;
	pthread_create(&listener_thread, NULL, signal_listener, (void*)&serverfd);
	pthread_create(&client_acceptor, NULL, accept_client, (void*)&serverfd);

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

void* accept_client(void* args)
{
	int serverfd = *((int*)args);
	int tmpfd = -1; // store fd of new connected node temporarily
	struct sockaddr_in connaddr;
	int addrlen = sizeof(connaddr);
	int readbytes;
	char* haddrp;

	// accepts connection from client repetitively
	while(1)
	{
		tmpfd = accept(serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		if(tmpfd < 0) // connection not received
			continue;
		else
		{
			fcntl(tmpfd, F_SETFL, O_NONBLOCK); // set socket to be non-blocking socket to avoid deadlock

			// send "whoareyou" message to connected node
			write(tmpfd, "whoareyou", BUF_SIZE);

			// get reply from connected node
			while(1) // break after reply received or connection abnormally closed
			{
				memset(read_buf_client, 0, BUF_SIZE);
				readbytes = read(tmpfd, read_buf_client, BUF_SIZE);
				if(readbytes == 0)
				{
					cout<<"[master]Connection closed from client before getting request"<<endl;

					close(tmpfd);
					tmpfd = -1;
					break;
				}
				else if(readbytes < 0)
				{
					// sleep for 0.01 second.  change this if necessdary
					usleep(10000);
					continue;
				}
				else // reply arrived
					break;
				// sleep for 0.01 second.  change this if necessdary
				usleep(10000);
			}
			if(strncmp(read_buf_client, "client", 6) == 0)
			{

				// limit the maximum available client connection
				if(clients.size() == max_client)
				{
					cout<<"[master]Cannot accept any more client request due to maximum client connection limit"<<endl;
					cout<<"[master]\tClosing connection from the client..."<<endl;
					write(tmpfd, "nospace", BUF_SIZE);
					close(tmpfd);
					break;
				}

				// get ip address of client
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("[master]Client node connected from %s \n", haddrp);

				clients.push_back(new connclient(tmpfd));
			}
			else if(strncmp(read_buf_client, "slave", 5) == 0)
			{
				cout<<"[master]Unexpected connection from slave: "<<endl;
				cout<<"[master]Closing connection to the slave..."<<endl;
				// check this code
				write(tmpfd, "close", BUF_SIZE);
				close(tmpfd);
			}
			else
			{
				cout<<"[master]Unidentified connected node: "<<endl;
				cout<<"[master]Closing connection to the node..."<<endl;
				close(tmpfd);
			}
		}
		// sleep 0.01 second for each loop
		// change this if necessary
		usleep(10000);
	}
}

void* signal_listener(void* args)
{
	int serverfd = *((int*)args);
	int readbytes = 0;
	int status; // for waitpid
	while(1)
	{
		for(int i=0; i<slaves.size(); i++) // listen to slaves
		{
			memset(read_buf_signal, 0, BUF_SIZE);
			readbytes = read(slaves[i]->getfd(), read_buf_signal, BUF_SIZE);
			if(readbytes == 0) // connection closed from slave
			{
				cout<<"[master]Connection from a slave closed"<<endl;
				// TODO: synchronize this
				delete slaves[i];
				slaves.erase(slaves.begin()+i);
				i--;
				continue;
			}
			else if(readbytes < 0)
				continue;
			else // signal from the slave
			{
				cout<<"[master]Undefined signal from slave "<<i<<": "<<read_buf_signal<<endl;
			}
		}
		for(int i=0; i<clients.size(); i++) // listen to clients
		{
			memset(read_buf_signal, 0, BUF_SIZE);
			readbytes = read(clients[i]->getfd(), read_buf_signal, BUF_SIZE);
			if(readbytes == 0) // connection closed from client
			{
				// remove file descriptor from the list
				cout<<"[master]Client connection closed"<<endl;
				
				delete clients[i];
				clients.erase(clients.begin()+i);
				i--;
				continue;
			}
			else if(readbytes < 0)
				continue;
			else // signal from the client
			{
				cout<<"[master]Message accepted from client: "<<read_buf_signal<<endl;
				if(strncmp(read_buf_signal, "stop", 4) == 0) // "stop" signal arrived
				{
					int currfd = clients[i]->getfd(); // store the current client's fd

					// stop all slave
					for(int j=0;j<slaves.size();j++)
					{
						write(slaves[j]->getfd(), "close", BUF_SIZE);

						// blocking read from slave
						while(true) // until the slave is closed
						{
							readbytes = read(slaves[j]->getfd(), read_buf_signal, BUF_SIZE);
							if(readbytes == 0) // closing slave succeeded
							{
								delete slaves[j];
								slaves.erase(slaves.begin()+j);
								j--;
								break;
							}
							else if(readbytes < 0)
							{
								// sleeps for 0.01 seconds. change this if necessary
								usleep(10000);
								continue;
							}
							else // message arrived before closed
								write(slaves[j]->getfd(),"ignored",BUF_SIZE);
						}
						cout<<"[master]Connection from a slave closed"<<endl;
					}
					cout<<"[master]All slaves closed"<<endl;

					// stop all client except the one requested stop
					for(int j=0;j<clients.size();j++)
					{
						if(currfd==clients[j]->getfd()) // except the client who requested the stop
							continue;

						write(clients[j]->getfd(), "close", BUF_SIZE);

						// blocking read from client
						while(true) // until the client is closed
						{
							readbytes = read(clients[j]->getfd(), read_buf_signal, BUF_SIZE);
							if(readbytes == 0) // closing client succeeded
							{
								delete clients[j];
								clients.erase(clients.begin()+j);
								j--;
								break;
							}
							else if(readbytes < 0)
							{
								// sleeps for 0.01 seconds. change this if necessary
								usleep(10000);
								continue;
							}
							else // message arrived before closed
								write(clients[j]->getfd(),"ignored",BUF_SIZE);
						}
					}
					cout<<"[master]All clients closed"<<endl;

					write(clients[i]->getfd(), "result: stopping successful", BUF_SIZE);
				}
				else if(strncmp(read_buf_signal, "numslave", 8) == 0) // "numslave" signal arrived
				{
					string ostring = "result: number of slave nodes = ";
					stringstream ss;
					ss<<slaves.size();
					ostring.append(ss.str());
					memset(write_buf_signal, 0, BUF_SIZE);
					strcpy(write_buf_signal, ostring.c_str());
					write(clients[i]->getfd(), write_buf_signal, BUF_SIZE);
				}
				else if(strncmp(read_buf_signal, "numclient", 9) == 0) // "numclient" signal arrived
				{
					string ostring = "result: number of client nodes = ";
					stringstream ss;
					ss<<clients.size();
					ostring.append(ss.str());
					memset(write_buf_signal, 0, BUF_SIZE);
					strcpy(write_buf_signal, ostring.c_str());
					write(clients[i]->getfd(), write_buf_signal, BUF_SIZE);
				}
				else if(strncmp(read_buf_signal, "submit", 6) == 0) // "submit" signal arrived assuming the program exist
				{
					char *buf_content = new char[sizeof(read_buf_signal)];
					strcpy(buf_content, read_buf_signal);

					jobs.push_back(new job(clients[i]));
					clients[i]->setrunningjob(jobs.back());
					run_job(buf_content, jobs.back());
				}
				else // undefined signal
				{
					cout<<"[master]Undefined signal from client: "<<read_buf_signal<<endl;
					write(clients[i]->getfd(), "result: error. the request is unknown", BUF_SIZE);
				}
			}
		}
		for(int i=0; i<jobs.size(); i++) // jobfds
		{
			memset(read_buf_signal, 0, BUF_SIZE);
			readbytes = read(jobs[i]->getreadfd(), read_buf_signal, BUF_SIZE);
			if(readbytes == 0) // pipe fd closed maybe process terminated
			{
				jobs[i]->getclient()->setrunningjob(NULL);
				delete jobs[i];
				jobs.erase(jobs.begin()+i);
				i--;
				cout<<"[master]Job fds closed unexpectedly"<<endl;
			}
			else if(readbytes < 0)
				continue;
			else // signal from the job
			{
				cout<<"[master]Message accepted from job "<<jobs[i]->getjobpid()<<": "<<read_buf_signal<<endl;
				if(strncmp(read_buf_signal, "whatisrole", 10) == 0) // "whatisrole" signal arrived
				{
					write(jobs[i]->getwritefd(), "job", BUF_SIZE);
				}
				else if(strncmp(read_buf_signal, "succompletion", 13) == 0) // "succompletion" signal arrived
				{
					cout<<"[master]Job "<<jobs[i]->getjobpid()<<" successfully completed"<<endl;
					stringstream ss;
					ss<<"result: Job "<<jobs[i]->getjobpid()<<" successfully completed";
					memset(write_buf_signal, 0, BUF_SIZE);
					strcpy(write_buf_signal, ss.str().c_str());
					write(jobs[i]->getclient()->getfd(), write_buf_signal, BUF_SIZE);

					// clear up the completed job
					write(jobs[i]->getwritefd(), "terminate", BUF_SIZE);

					jobs[i]->getclient()->setrunningjob(NULL);
					delete jobs[i];
					jobs.erase(jobs.begin()+i);
					i--;
				}
				else // undefined signal
				{
					cout<<"[master]Undefined signal from job: "<<read_buf_signal<<endl;
				}
			}
		}
		// check unexpected termination of jobs
		for(int i=0; i<jobs.size(); i++)
		{
			if(waitpid(jobs[i]->getjobpid(), &status, WNOHANG) > 0) // job terminated unexpectedly
			{
				cout<<"[master]Job "<<jobs[i]->getjobpid()<<" terminated without successfulcompletion message"<<endl;
				stringstream ss;
				ss<<"result: Job "<<jobs[i]->getjobpid()<<" terminated unsuccessfully";
				memset(write_buf_signal, 0, BUF_SIZE);
				strcpy(write_buf_signal, ss.str().c_str());
				write(clients[i]->getfd(), write_buf_signal, BUF_SIZE);
				
				jobs[i]->getclient()->setrunningjob(NULL);
				delete jobs[i];
				jobs.erase(jobs.begin()+i);
				i--;
			}
		}

		// break if all slaves and clients are closed
		if(slaves.size() == 0 && clients.size() == 0)
			break;

		// sleeps for 0.01 seconds. change this if necessary
		usleep(10000);
	}

	// close master socket
	close(serverfd);
	cout<<"[master]Master server closed"<<endl;

	cout<<"[master]Exiting master..."<<endl;

	exit(0);
}

void run_job(char* buf_content, job* thejob)
{
	int pid;
	int fd1[2]; // two set of fds between master and job(1)
	int fd2[2]; // two set of fds between master and job(2)
	pipe(fd1); // fd1[0]: master read, fd1[1]: job write
	pipe(fd2); // fd2[0]: job read, fd2[1]: master write

	// set pipe fds to be non-blocking to avoid deadlock
	fcntl(fd1[0], F_SETFL, O_NONBLOCK);
	fcntl(fd1[1], F_SETFL, O_NONBLOCK);
	fcntl(fd2[0], F_SETFL, O_NONBLOCK);
	fcntl(fd2[1], F_SETFL, O_NONBLOCK);

	thejob->setreadfd(fd1[0]);
	thejob->setwritefd(fd2[1]);
	thejob->setpipefds(fd1[1], fd2[0]);

	pid = fork();

	if(pid == 0)
	{
		char* token;
		char* argvalues[BUF_SIZE]; // maximum number of arguments limited to BUF_SIZE
		int argcount = 1;
		token = strtok(buf_content, " "); // token -> submit
		token = strtok(NULL, " "); // token -> program name
		string program = token;
		string path = MR_PATH;
		path.append(program); // path constructed
		argvalues[0] = new char[strlen(path.c_str())+1];
		strcpy(argvalues[0], path.c_str());

		while(1) // parse all arguments
		{
			token = strtok(NULL, " ");
			if(token == NULL)
				break;
			else
			{
				argvalues[argcount] = new char[sizeof(token)];
				strcpy(argvalues[argcount], token);
				argcount++;
			}
		}
		free(buf_content);

		// pass the pipefds
		stringstream ss1, ss2;
		ss1<<fd2[0];
		argvalues[argcount] = new char[strlen(ss1.str().c_str())+1];
		strcpy(argvalues[argcount], ss1.str().c_str());
		argcount++;
		ss2<<fd1[1];
		argvalues[argcount] = new char[strlen(ss2.str().c_str())+1];
		strcpy(argvalues[argcount], ss2.str().c_str());
		argcount++;

		argvalues[argcount] = NULL;
		execv(argvalues[0], argvalues);
	}
	else if(pid < 0)
	{
		cout<<"[master]Job could not be started due to child process forking failure"<<endl;
	}
	else
	{
		char* token;
		token = strtok(buf_content, " "); // token -> submit
		token = strtok(NULL, " "); // token -> program name
		string program = token;
		thejob->setjobpid(pid);
		cout<<"[master]Job "<<thejob->getjobpid()<<" starting: "<<program<<endl;
		free(buf_content);
	}
}
