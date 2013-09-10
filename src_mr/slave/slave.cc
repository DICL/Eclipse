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

#define PORT 7000
#define BUF_SIZE 32

using namespace std;

fd_set fdslist; // list of file descriptor for select()
fd_set selectfds; // list of file descriptor for select()
int maxfd = 0; // keep track of maximum file descriptor for use of select()
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
		cout<<"\topenning socket failed"<<endl;

	hp = gethostbyname(host);

	if (hp == NULL)
		cout<<"\tcannot find host by host name"<<endl;

	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
	return clientfd;
}

int get_signal()
{
	cout<<"ABC";
}

int send_signal()
{
	cout<<"ABC";
}


int main(int argc, char** argv)
{
	FD_ZERO(&fdslist);
	FD_ZERO(&selectfds);
	// usage
	if(argc<=1)
	{
		cout<<"\tmaster address is not specified"<<endl;
		cout<<"\tusage: slave [master address]"<<endl;
		cout<<"\texiting..."<<endl;
		return 1;
	}


	// connect to master
	int masterfd = connect_to_server(argv[1], PORT);
	if(masterfd<0)
	{
		cout<<"\tconnecting to master failed"<<endl;
		return 1;
	}

	// add the file descriptor of standard input
	FD_SET(0, &fdslist);
	// add the file descriptor to the fdslist and update maxfd
	FD_SET(masterfd, &fdslist);
	if(masterfd>maxfd)
		maxfd = masterfd;

	// set master socket to be non-blocking socket
	fcntl(masterfd, F_SETFL, O_NONBLOCK);

	// get signal from master through select()
	int readbytes = 0;
	while(1)
	{
		selectfds = fdslist;
		if(select(maxfd+1, &selectfds, NULL, NULL, NULL)<0)
			cout<<"select error"<<endl;
		if(FD_ISSET(masterfd, &selectfds))
		{
			// if an signal arrived from master
			readbytes = read(masterfd, read_buf, BUF_SIZE);
			if(readbytes == 0)
			{
				// connection closed
				cout<<"\tconnection from master is abnormally closed"<<endl;
				if(close(masterfd)<0)
					cout<<"\tclosing socket failed"<<endl;
				else
					cout<<"\tmaster socket closed"<<endl;

				return 0;
			}
			else
			{
				// connection arrived from master
				cout<<"signal from master: "<<read_buf<<endl;
			}
		}
		if(FD_ISSET(0,&selectfds))
		{
			// input from keyboard arrived
			readbytes = read(0, read_buf, BUF_SIZE);
			if(strncmp(read_buf, "exit", 4) == 0)
			{
				cout<<"exit request from terminal"<<endl;
				break;
			}
			else
				cout<<"signal from keyboard: "<<read_buf<<endl;
		}
		// sleeps for 0.01 seconds. change this if necessary
		//usleep(10000);
		usleep(1000000);
		if(write(masterfd, "hello master", BUF_SIZE) < 0)
			cout<<"sending signal to master blocked"<<endl;
		cout<<"while loop iteration"<<endl;
	}
	if(close(masterfd)<0)
		cout<<"close failed"<<endl;

	return 0;
}
