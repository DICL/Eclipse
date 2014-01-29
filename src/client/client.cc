#include "client.hh"
#include <iostream>
#include <fstream>
#include <string>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <mapreduce/definitions.hh>

using namespace std;

char read_buf[BUF_SIZE];
char write_buf[BUF_SIZE];
char master_address[BUF_SIZE];
int port = -1;
int masterfd = -1;
int master_readbytes = 0;
bool master_is_set = false;

int main(int argc, char** argv)
{
	// usage
	if(argc<2)
	{
		cout<<"Insufficient arguments: at least 1 argument needed"<<endl;
		cout<<"usage: client [request]"<<endl;
		cout<<"Exiting..."<<endl;
		return 1;
	}

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
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "port")
		{
			conf>>token;
			port = atoi(token.c_str());
		}
		else if(token == "max_job")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "num_slave")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "master_address")
		{
			conf>>token;
			strcpy(master_address, token.c_str());
			master_is_set = true;
		}
		else
		{
			cout<<"[client]Unknown configure record: "<<token<<endl;
		}
		conf>>token;
	}
	conf.close();
	// verify initialization
	if(port == -1)
	{
		cout<<"[client]port should be specified in the setup.conf"<<endl;
		exit(1);
	}
	if(master_is_set == false)
	{
		cout<<"[client]master_address should be specified in the setup.conf"<<endl;
		exit(1);
	}
	// copy request command to write buffer
	if(strncmp(argv[1], "stop", 4) == 0)
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf,"stop");
	}
	else if(strncmp(argv[1], "numslave", 8) == 0)
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, "numslave");
	}
	else if(strncmp(argv[1], "numclient", 9) == 0)
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, "numclient");
	}
	else if(strncmp(argv[1], "numjob", 6) == 0)
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, "numjob");
	}
	else if(strncmp(argv[1], "help", 4) == 0)
	{
		// TODO: lists request and their usage
	}
	/*
	else if(strncmp(argv[1], "submit", 6) == 0) // submit a job
	{
		// compile the submitted job
		if(argc<3)
		{
			cout<<"The file name to submit is missing"<<endl;
			cout<<"usage: client submit [program path]"<<endl;
			cout<<"Exiting..."<<endl;
			return 1;
		}

		string args = argv[2];

		string writestring = "submit ";
		writestring.append(args);

		for(int i=3; i<argc; i++)
		{
			args = argv[i];
			writestring.append(" ");
			writestring.append(args);
		}

		// TODO: argument passing should be implemented when needed

		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, writestring.c_str());

		// TODO: Check if the file exist and if it's executable
		cout<<"Submitting the job..."<<endl;
	}
	*/
	else
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, argv[1]);
	}

	masterfd = connect_to_server(master_address, port);
	if(masterfd<0)
	{
		cout<<"Connecting to master failed"<<endl;
		exit(1);
	}

	// set sockets to be non-blocking socket to avoid deadlock
	fcntl(masterfd, F_SETFL, O_NONBLOCK);

	// start listener thread
	pthread_t listener_thread;
	pthread_create(&listener_thread, NULL, signal_listener, (void*)&masterfd);


	// sleeping loop which prevents process termination
	while(1)
		sleep(1);

	return 0;
}

int connect_to_server(char *host, unsigned short port)
{
	int clientfd;
	struct sockaddr_in serveraddr;
	struct hostent *hp;

	// SOCK_STREAM -> tcp
	clientfd = socket(AF_INET, SOCK_STREAM, 0);
	if(clientfd<0)
	{
		cout<<"Openning socket failed"<<endl;
		exit(1);
	}

	hp = gethostbyname(host);

	if (hp == NULL)
	{
		cout<<"Cannot find host by host name"<<endl;
		return -1;
	}

	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
	return clientfd;
}

void* signal_listener(void* args)
{
	int serverfd = *((int*)args);
	int readbytes = 0;
	char tmp_buf[BUF_SIZE];
	while(1)
	{
		// listen to the matser node

		// set the read buffer
		if(master_readbytes == 0)
			memset(read_buf, 0, BUF_SIZE);

		readbytes = read(serverfd, read_buf+master_readbytes, BUF_CUT-master_readbytes%BUF_CUT); // non-blocking read
		if(readbytes == 0) // connection closed from master
		{
			cout<<"Connection from master is abnormally closed"<<endl;
			close(serverfd);
			exit(0);
		}
		else if(readbytes < 0) // no signal arrived
		{
			continue;
		}
		else if(read_buf[master_readbytes+readbytes-1] != 0
			|| (master_readbytes+readbytes)%BUF_CUT != 0)
		{
			master_readbytes = master_readbytes + readbytes;
		}
		else // a signal arrived from master
		{
			// update the readbytes
			master_readbytes = 0;

			if(strncmp(read_buf, "whoareyou", 9) == 0)
			{
				// respond to "whoareyou"
				memset(tmp_buf, 0, BUF_SIZE);
				strcpy(tmp_buf, "client");
				write(serverfd, tmp_buf, BUF_CUT*(strlen(tmp_buf)/BUF_CUT+1));

				// request to master
				write(serverfd, write_buf, BUF_CUT*(strlen(write_buf)/BUF_CUT+1));
			}
			else if(strncmp(read_buf, "close", 5) == 0)
			{
				cout<<"Close request from master"<<endl;
				close(serverfd);
				cout<<"Exiting client..."<<endl;
				exit(0);
			}
			else if(strncmp(read_buf, "result", 6) == 0)
			{
				cout<<read_buf<<endl;
				close(serverfd);
				exit(0);
			}
			else
			{
				cout<<"Signal from master: "<<read_buf<<endl;
			}
		}

		// sleeps for 0.0001 seconds. change this if necessary
		// usleep(100);
	}
	close(serverfd);

	cout<<"Exiting client..."<<endl;
	exit(0);
}
