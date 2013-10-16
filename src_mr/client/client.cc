#include <iostream>
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

#define PORT 7006
#define BUF_SIZE 256
#define MR_PATH "/home/youngmoon01/MRR_storage/"
#define LIB_PATH "/home/youngmoon01/MRR/MRR/src_mr/"


using namespace std;

char read_buf[BUF_SIZE];
char write_buf[BUF_SIZE];

int main(int argc, char** argv)
{
	// usage
	if(argc<3)
	{
		cout<<"Insufficient arguments: at least 2 arguments needed"<<endl;
		cout<<"usage: client [master address] [request]"<<endl;
		cout<<"Exiting..."<<endl;
		return 1;
	}
	// copy request command to write buffer
	if(strncmp(argv[2], "stop", 4) == 0)
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf,"stop");
	}
	else if(strncmp(argv[2], "numslave", 8) == 0)
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, "numslave");
	}
	else if(strncmp(argv[2], "numclient", 9) == 0)
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, "numclient");
	}
	else if(strncmp(argv[2], "submit", 6) == 0) // submit a job
	{
		// compile the submitted job
		if(argc<4)
		{
			cout<<"The file name to submit is missing"<<endl;
			cout<<"usage: client [master address] submit [source code]"<<endl;
			cout<<"Exiting..."<<endl;
			return 1;
		}
		int pid;
		int status;
		pid = fork();
		if(pid == 0)
		{
			cout<<"Compiling the code..."<<endl;
			//TODO: check if the source file exist and deal with the compile error case

			char *arg = (char*)malloc(sizeof(argv[3]));
			strcpy(arg, argv[3]);
			char *name;
			name = strtok(arg, ".");

			string inputpath = MR_PATH;
			inputpath.append(argv[3]);
			string outputpath = MR_PATH;
			outputpath.append(name);
			outputpath.append(".out");

			execl("/usr/bin/g++", "/usr/bin/g++", inputpath.c_str(),
				"-o", outputpath.c_str(), "-I", LIB_PATH, NULL);
			return 0;
		}
		else if(pid < 0)
		{
			cout<<"Compilation failed due to failure of forking child process."<<endl;
			return 1;
		}

		char *arg = (char*)malloc(sizeof(argv[3]));
		strcpy(arg, argv[3]);
		char *name;
		name = strtok(arg, ".");
		string program = name;
		program.append(".out");

		free(arg);


		string writestring = "submit ";
		writestring.append(program);

		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, writestring.c_str());

		waitpid(pid, &status, 0);
		// TODO: ensure the compilation and check if the file exist
		cout<<"Compiling done..."<<endl;
		cout<<"Submitting job..."<<endl;
	}
	else
	{
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, argv[2]);
	}
	
	int masterfd = connect_to_server(argv[1], PORT);
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

void *signal_listener(void *args)
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
