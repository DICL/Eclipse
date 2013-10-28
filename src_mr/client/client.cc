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
#include "client.hh"
#include <mapreduce/mapreduce.hh>

using namespace std;

char read_buf[BUF_SIZE];
char write_buf[BUF_SIZE];
char master_address[BUF_SIZE];
int port = -1;
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

	while(1)
	{
		conf>>token;
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
		else if(token == "max_client")
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
		else if(token == "end")
		{
			break;
		}
		else
		{
			cout<<"[client]Unknown configure record: "<<token<<endl;
		}
	}
	conf.close();
	// verify initialization
	if(port == -1)
	{
		cout<<"[client]port should be specified in the setup.conf"<<endl;
		return 1;
	}
	if(master_is_set == false)
	{
		cout<<"[client]master_address should be specified in the setup.conf"<<endl;
		return 1;
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
	else if(strncmp(argv[1], "help", 4) == 0)
	{
		// TODO: lists request and their usage
	}
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

		string program = argv[2]; // program file name

		string writestring = "submit ";
		writestring.append(program);

		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, writestring.c_str());

		// TODO: Check if the file exist and if it's executable
		cout<<"Submitting job..."<<endl;
	}
	else
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, argv[1]);
	}
	
	int masterfd = connect_to_server(master_address, port);
	if(masterfd<0)
	{
		cout<<"Connecting to master failed"<<endl;
		return 1;
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

	//SOCK_STREAM -> tcp
	clientfd = socket(AF_INET, SOCK_STREAM, 0);
	if(clientfd<0)
		cout<<"Openning socket failed"<<endl;
	
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
	int masterfd = *((int*)args);
	int readbytes = 0;
	while(1)
	{
		memset(read_buf, 0, BUF_SIZE);
		readbytes = read(masterfd, read_buf, BUF_SIZE); // non-blocking read
		if(readbytes == 0) // connection closed from master
		{
			cout<<"Connection from master is abnormally closed"<<endl;
			if(close(masterfd)<0)
				cout<<"Closing socket failed"<<endl;
			else
			{
				cout<<"Connection to master closed"<<endl;
			}
			exit(0);
		}
		else if(readbytes < 0) // no signal arrived
			continue;
		else // a signal arrived from master
		{
			if(strncmp(read_buf, "whoareyou", 9) == 0)
			{
				// respond to "whoareyou"
				write(masterfd, "client", BUF_SIZE);

				// request to master
				write(masterfd, write_buf, BUF_SIZE);
			}
			else if(strncmp(read_buf, "close", 5) == 0)
			{
				cout<<"Close request from master"<<endl;
				if(close(masterfd)<0)
					cout<<"Close failed"<<endl;
				cout<<"Exiting client..."<<endl;
				exit(0);
			}
			else if(strncmp(read_buf, "result", 6) == 0)
			{
				cout<<read_buf<<endl;
				if(close(masterfd)<0)
					cout<<"Close failed"<<endl;
				exit(0);
			}
			else
			{
				cout<<"Signal from master: "<<read_buf<<endl;
			}
		}
		// sleeps for 0.01 seconds. change this if necessary
		usleep(10000);
	}
	if(close(masterfd)<0)
		cout<<"Close failed"<<endl;

	cout<<"Exiting client..."<<endl;
	exit(0);
}
