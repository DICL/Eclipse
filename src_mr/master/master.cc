#include <iostream>
#include <fstream>
#include <string>
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
#include <mapreduce/mapreduce.hh>

#define BUF_SIZE 256
#define MR_PATH "/home/youngmoon01/MRR_storage/"
#define LIB_PATH "/home/youngmoon01/MRR/MRR/src_mr/"

using namespace std;

int conn_slave = 0;
int conn_client = 0;
int client_clock = 0;
int *slavefds;
int *clientfds;
int **jobfds; // jobfds[jobid][0]: read, jobfds[jobid][1]: write
int *jobpids; // keep track of jobids

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

	slavefds = new int[num_slave];
	clientfds = new int[max_client];
	jobfds = new int*[max_client];
	for(int i=0;i<max_client;i++)
		jobfds[i] = new int[2];

	jobpids = new int[max_client];
	for(int i=0;i<num_slave;i++)
		slavefds[i] = -1;
	for(int i=0;i<max_client;i++)
	{
		clientfds[i] = -1;
		jobfds[i][0] = -1;
		jobfds[i][1] = -1;
		jobpids[i] = -1;
	}


	int serverfd = open_server(port);
	if(serverfd < 0)
	{
		cout<<"[master]Openning server failed"<<endl;
		return 1;
	}

	struct sockaddr_in connaddr;
	int addrlen = sizeof(connaddr);
	char *haddrp;

	while(1)
	{
		slavefds[conn_slave] = accept(serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		if(slavefds[conn_slave] < 0)
		{
			cout<<"[master]Accepting failed"<<endl;
			continue;
		}
		else
		{
			// check if the accepted node is slave or client
			write(slavefds[conn_slave], "whoareyou", BUF_SIZE);
			read(slavefds[conn_slave], read_buf_client, BUF_SIZE);
			if(strncmp(read_buf_client, "slave", 5) == 0) // slave connected
			{
				// increase the number of connected slave
				conn_slave++;

				// get ip address of slave
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("slave node connected from %s \n", haddrp);
			}
			else if(strncmp(read_buf_client, "client", 6) == 0)// client connected
			{
				// TODO: limit the number of client to allowed value
				clientfds[client_clock] = slavefds[conn_slave];
				slavefds[conn_slave] = -1;

				// set sockets to be non-blocking socket to avoid deadlock
				fcntl(clientfds[client_clock], F_SETFL, O_NONBLOCK);

				// increase number of connected client
				client_clock++;
				conn_client++;

				// get ip address of client
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("client node connected from %s \n", haddrp);

			}
			else // unknown connection
			{
				// TODO: deal with this case
				cout<<"[master]Unknown connection"<<endl;
			}

			if(conn_slave == num_slave)
			{
				cout<<"[master]All slave nodes are connected successfully"<<endl;
				break;
			}
			else if(conn_slave>num_slave)
				cout<<"[master]Number of slave connection exceeded allowed limits"<<endl;
		}
		// sleeps for 0.01 seconds. change this if necessary
		usleep(10000);
	}

	// set sockets to be non-blocking socket to avoid deadlock
	fcntl(serverfd, F_SETFL, O_NONBLOCK);
	for(int i=0;i<num_slave;i++)
		fcntl(slavefds[i], F_SETFL, O_NONBLOCK);

	// send the message to slave for debugging
	for(int i=0; i<num_slave; i++)
		write(slavefds[i], "hello slaves", 32);

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

void *accept_client(void *args)
{
	int serverfd = *((int*)args);
	int tmpfd = -1; // file descriptor to store fd of new connected node temporarily
	struct sockaddr_in connaddr;
	int addrlen = sizeof(connaddr);
	int readbytes;
	char *haddrp;

	while(1)
	{
		tmpfd = accept(serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		if(tmpfd < 0) // connection not received
			continue;
		else
		{
			fcntl(tmpfd, F_SETFL, O_NONBLOCK); // set socket ot be non-blocking socket to avoid deadlock
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
					if(close(tmpfd)<0);
						cout<<"[master]Closing socket failed"<<endl;
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
				// get ip address of client
				haddrp = inet_ntoa(connaddr.sin_addr);
				printf("[master]client node connected from %s \n", haddrp);

				clientfds[client_clock] = tmpfd;

				tmpfd = -1;

				// increase the number of connected client
				conn_client++;

				// determine new available client fd slot
				client_clock = -1;
				while(client_clock == -1)
				{
					for(int i=0; i<max_client; i++)
					{
						if(clientfds[i] == -1) // the slot is available
						{
							client_clock = i;
							break;
						}
						else if(i == max_client-1) // all slots are occupied
						{
							// sleep 1 second to wait until a client finish its job
							// and run the for loop again
							// change the sleep time if necessary
							usleep(1000000);
							break;
						}
					}
				}
			}
			else if(strncmp(read_buf_client, "slave", 5) == 0)
			{
				cout<<"[master]Unexpected connection from slave: "<<endl;
				cout<<"[master]Closing connection to the slave..."<<endl;
				// check this code
				write(tmpfd, "close", 5);
				if(close(tmpfd)<0)
					cout<<"[master]Closing socket failed"<<endl;

				tmpfd = -1;
			}
			else
			{
				// TODO: deal with this case
				cout<<"[master]Unidentified connected node: "<<endl;
				cout<<"[master]Closing connection to the node..."<<endl;
				if(close(tmpfd)<0)
					cout<<"[master]Closing socket failed"<<endl;
			}
		}
		// sleep 0.01 second for each loop
		// change this if necessary
		usleep(10000);
	}
}

void *signal_listener(void *args)
{
	int serverfd = *((int*)args);
	int readbytes = 0;
	int status; // for waitpid
	while(1)
	{
		for(int i=0; i<num_slave; i++) // slavefds
		{
			while(slavefds[i] != -1) // TODO: check if changing to if~continue is okay
			{
				memset(read_buf_signal, 0, BUF_SIZE);
				readbytes = read(slavefds[i], read_buf_signal, BUF_SIZE);
				if(readbytes == 0) // connection closed from slave
				{
					if(close(slavefds[i])<0)
						cout<<"[master]Closing slave "<<i<<" connection failed"<<endl;
					else 
					{
						// count closed slaves and remove file descriptor from the list
						cout<<"[master]Slave "<<i<<" closed"<<endl;
						slavefds[i] = -1;
						// TODO: synchronize this
						conn_slave--;
					}
				}
				else if(readbytes < 0)
					break;
				else // signal from the slave
				{
					cout<<"[master]Undefined signal from slave "<<i<<": "<<read_buf_signal<<endl;
				}
			}
		}
		for(int i=0; i<max_client; i++) // clientfds
		{
			while(clientfds[i] != -1) // TODO: check if changing to if~continue is okay
			{
				memset(read_buf_signal, 0, BUF_SIZE);
				readbytes = read(clientfds[i], read_buf_signal, BUF_SIZE);
				if(readbytes == 0) // connection closed from client
				{
					close(clientfds[i]);
					// remove file descriptor from the list
					cout<<"[master]Client connection closed"<<endl;
					conn_client--;
					clientfds[i] = -1;
					close(jobfds[i][0]);
					close(jobfds[i][1]);
					jobfds[i][0] = -1;
					jobfds[i][1] = -1;
					jobpids[i] = -1; // initialize job id

				}
				else if(readbytes < 0)
					break;
				else // signal from the client
				{
					cout<<"[master]Message accepted from client: "<<read_buf_signal<<endl;
					if(strncmp(read_buf_signal, "stop", 4) == 0) // "stop" signal arrived
					{
						// stop all slave
						for(int j=0;j<num_slave;j++)
						{
							if(slavefds[j] != -1) // TODO: check if changing to if~continue is okay
							{
								write(slavefds[j], "close", BUF_SIZE);

								// blocking read from slave
								while(slavefds[j]!=-1) // until slave is closed
								{
									readbytes = read(slavefds[j], read_buf_signal, BUF_SIZE);
									if(readbytes == 0) // closing slave succeeded
									{
										if(close(slavefds[j])<0)
										{
											cout<<"[master]Closing slave failed"<<endl;
											slavefds[j] = -1;
											// TODO: synchronize this
											conn_slave--;
										}
										else
										{
											slavefds[j] = -1;
											// TODO: synchronize this
											conn_slave--;
										}
									}
									else if(readbytes < 0)
									{
										// sleeps for 0.01 seconds. change this if necessary
										usleep(10000);
										continue;
									}
									else // message arrived before closed
										write(slavefds[j],"ignored",BUF_SIZE);

									// sleeps for 0.01 seconds. change this if necessary
									usleep(10000);
								}
								cout<<"[master]Slave "<<j<<" closed"<<endl;
							}
						}
						cout<<"[master]All slaves closed"<<endl;

						// stop all client except the one requested stop
						for(int j=0;j<max_client;j++)
						{
							if(i!=j && clientfds[j] != -1) // except the client, TODO: check if changing to if~continue is okay
							{
								write(clientfds[j], "close", BUF_SIZE);

								// blocking read from client
								while(clientfds[j]!=-1)
								{
									readbytes = read(clientfds[j], read_buf_signal, BUF_SIZE);
									if(readbytes == 0) // closing client succeeded
									{
										if(close(clientfds[j])<0)
											cout<<"[master]Closing client failed"<<endl;
										else
											clientfds[j] = -1;
									}
									else if(readbytes < 0)
									{
										// sleeps for 0.01 seconds. change this if necessary
										usleep(10000);
										continue;
									}
									else // message arrived before closed
										write(clientfds[j],"ignored",BUF_SIZE);

									// sleeps for 0.01 seconds. change this if necessary
									usleep(10000);
								}
							}
						}
						cout<<"[master]All clients closed"<<endl;

						write(clientfds[i], "result: stopping successful", BUF_SIZE);
					}
					else if(strncmp(read_buf_signal, "numslave", 8) == 0) // "numslave" signal arrived
					{
						string ostring = "result: number of slave nodes = ";
						stringstream ss;
						ss<<conn_slave;
						ostring.append(ss.str());
						memset(write_buf_signal, 0, BUF_SIZE);
						strcpy(write_buf_signal, ostring.c_str());
						write(clientfds[i], write_buf_signal, BUF_SIZE);
					}
					else if(strncmp(read_buf_signal, "numclient", 9) == 0) // "numclient" signal arrived
					{
						string ostring = "result: number of client nodes = ";
						stringstream ss;
						ss<<conn_client;
						ostring.append(ss.str());
						memset(write_buf_signal, 0, BUF_SIZE);
						strcpy(write_buf_signal, ostring.c_str());
						write(clientfds[i], write_buf_signal, BUF_SIZE);
					}
					else if(strncmp(read_buf_signal, "submit", 6) == 0) // "submit" signal arrived assuming the program exist
					{
						char *buf_content = new char[(sizeof(read_buf_signal))];
						strcpy(buf_content, read_buf_signal);

						run_job(buf_content, i);
					}
					else // undefined signal
					{
						cout<<"[master]Undefined signal from client: "<<read_buf_signal<<endl;
						write(clientfds[i], "result: error. the request is unknown", BUF_SIZE);
					}
				}
			}
		}
		for(int i=0; i<max_client; i++) // jobfds
		{
			while(jobfds[i][0] != -1) // TODO: check if changing if~continue is okay
			{
				memset(read_buf_signal, 0, BUF_SIZE);
				readbytes = read(jobfds[i][0], read_buf_signal, BUF_SIZE);
				if(readbytes == 0) // pipe fd closed maybe process terminated
				{
					close(jobfds[i][0]);
					close(jobfds[i][1]);
					cout<<"[master]Job fds closed"<<endl;
					jobfds[i][0] = -1;
					jobfds[i][1] = -1;
				}
				else if(readbytes < 0)
					break;
				else // signal from the job
				{
					cout<<"[master]Message accepted from job "<<jobpids[i]<<": "<<read_buf_signal<<endl;
					if(strncmp(read_buf_signal, "whatisrole", 10) == 0) // "whatisrole" signal arrived
					{
						write(jobfds[i][1], "master", BUF_SIZE);
					}
					else if(strncmp(read_buf_signal, "successfulcompletion", 20) == 0) // "successfulcompletion" signal arrived
					{
						cout<<"[master]Job "<<jobpids[i]<<" successfully completed"<<endl;
						stringstream ss;
						ss<<"result: Job "<<jobpids[i]<<" successfully completed";
						memset(write_buf_signal, 0, BUF_SIZE);
						strcpy(write_buf_signal, ss.str().c_str());
						write(clientfds[i], write_buf_signal, BUF_SIZE);

						// clear up the completed job
						jobpids[i] = -1;
						write(jobfds[i][1], "terminate", BUF_SIZE);
					}
					else // undefined signal
					{
						cout<<"[master]Undefined signal from job: "<<read_buf_signal<<endl;
					}
				}
			}
		}
		// check unexpected termination of jobs
		for(int i=0; i<max_client; i++)
		{
			// continue if job is not executed
			if(jobpids[i]==-1)
				continue;
			
			if(waitpid(jobpids[i], &status, WNOHANG) > 0)
			{
				cout<<"[master]Job "<<jobpids[i]<<" terminated without successfulcompletion message"<<endl;
				stringstream ss;
				ss<<"result: Job "<<jobpids[i]<<" terminated unsuccessfully";
				memset(write_buf_signal, 0, BUF_SIZE);
				strcpy(write_buf_signal, ss.str().c_str());
				write(clientfds[i], write_buf_signal, BUF_SIZE);
			}
		}

		// break if all slaves and clients are closed
		if(conn_slave == 0 && conn_client == 0)
			break;

		// sleeps for 0.01 seconds. change this if necessary
		usleep(10000);
	}

	// close master socket
	if(close(serverfd)<0)
		cout<<"[master]Closing socket failed"<<endl;
	else
		cout<<"[master]Master closed"<<endl;

	// delete all dynamically allocated data
	delete [] slavefds;

	cout<<"[master]Exiting master..."<<endl;

	exit(0);
}

void run_job(char* buf_content, int clientnum)
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

	jobfds[clientnum][0] = fd1[0];
	jobfds[clientnum][1] = fd2[1];

	pid = fork();

	if(pid == 0)
	{
		char *token;
		char *argvalues[BUF_SIZE]; // maximum number of arguments limited to BUF_SIZE
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
		char *token;
		token = strtok(buf_content, " "); // token -> submit
		token = strtok(NULL, " "); // token -> program name
		string program = token;
		jobpids[clientnum] = pid;
		cout<<"[master]Job "<<jobpids[clientnum]<<" starting: "<<program<<endl;
		free(buf_content);
	}
}
