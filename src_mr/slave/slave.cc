#include <iostream>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include "slave.hh"

#define PORT 7006
#define BUF_SIZE 256

using namespace std;

char read_buf[BUF_SIZE];
char write_buf[BUF_SIZE];

int connect_to_server(char *host, unsigned short port)
{
	int clientfd;
	struct sockaddr_in serveraddr;
	struct hostent *hp;

	//SOCK_STREAM -> tcp
	clientfd = socket(AF_INET, SOCK_STREAM, 0);
	if(clientfd < 0)
		cout<<"[slave]openning socket failed"<<endl;

	hp = gethostbyname(host);

	if (hp == NULL)
		cout<<"[slave]cannot find host by host name"<<endl;

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
	// get signal from master through select()
	int readbytes = 0;
	while(1)
	{
		// if an signal arrived from master
		memset(read_buf, 0, BUF_SIZE);
		readbytes = read(masterfd, read_buf, BUF_SIZE);
		if(readbytes == 0) //connection closed from master
		{
			cout<<"[slave]connection from master is abnormally closed"<<endl;
			if(close(masterfd)<0)
				cout<<"[slave]closing socket failed"<<endl;
			cout<<"[slave]exiting slave..."<<endl;
			exit(0);
		}
		else if(readbytes < 0)
			continue;
		else // signal arrived from master
		{
			if(strncmp(read_buf, "whoareyou", 9) == 0)
			{
				write(masterfd, "slave", BUF_SIZE);
			}
			else if(strncmp(read_buf, "close", 5) == 0)
			{
				cout<<"[slave]close request from master"<<endl;
				if(close(masterfd<0))
					cout<<"[slave]close failed"<<endl;
				cout<<"[slave]exiting slave..."<<endl;
				exit(0);
			}
			else
			{
				cout<<"[slave]undefined signal from master: "<<read_buf<<endl;
			}
		}
		// sleeps for 0.01 seconds. change this if necessary
		usleep(10000);
	}
	if(close(masterfd)<0)
		cout<<"[slave]close failed"<<endl;
	cout<<"[slave]exiting slave..."<<endl;
	exit(0);
}

int main(int argc, char** argv)
{
	// usage
	if(argc<=1)
	{
		cout<<"master address is not specified"<<endl;
		cout<<"usage: slave [master address]"<<endl;
		cout<<"exiting..."<<endl;
		return 1;
	}

	// connect to master
	int masterfd = connect_to_server(argv[1], PORT);
	if(masterfd<0)
	{
		cout<<"[slave]connecting to master failed"<<endl;
		return 1;
	}

	// set master socket to be non-blocking socket to avoid deadlock
	fcntl(masterfd, F_SETFL, O_NONBLOCK);

	// create listener thread
	pthread_t listener_thread;
	pthread_create(&listener_thread, NULL, signal_listener, (void*)&masterfd);

	// sleeping loop which prevents process termination
	while(1)
		sleep(1);

	return 0;
}
