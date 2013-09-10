#include <iostream>
#include <sys/unistd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <netdb.h>
#include "master.hh"

#define BACKLOG 10
#define PORT 7000
#define BUF_SIZE 32

using namespace std;

fd_set fdslist; // list of file descriptor for select()
fd_set selectfds; // temp file descriptor list for select()
int maxfd = 0; // keep track of maximum file descriptor for use of select()
int num_slave = 0;
int *slavefds;
char **read_bufs;
char write_buf[BUF_SIZE];

int open_server(int port)
{
	int serverfd;
	struct sockaddr_in serveraddr;

	// socket open
	serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if(serverfd<0)
		cout<<"socket opening failed"<<endl;

	// bind
	memset((void *) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) port);
	if (bind(serverfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		cout<<"binding failed"<<endl;
		return -1;
	}

	// listen
	if(listen(serverfd, BACKLOG) < 0)
	{
		cout<<"listening failed"<<endl;
		return -1;
	}

	return serverfd;
}

int get_signal()
{
	cout<<"ABC";
}

// send signal
int send_signal()
{
	cout<<"ABC";
}


int main(int argc, char** argv)
{
	int conn_num_slave = 0;

	FD_ZERO(&selectfds);
	FD_ZERO(&fdslist);

	// usage
	if(argc>1)
		num_slave = atoi(argv[1]);
	else
	{
		cout<<"Number of slave nodes is not specified"<<endl;
		cout<<"usage: master [numer of slave nodes]"<<endl;
		cout<<"exiting..."<<endl;
		return 1;
	}
	slavefds = new int[num_slave];
	read_bufs = new char*[num_slave];
	for(int i=0;i<num_slave;i++)
	{
		read_bufs[i] = new char[BUF_SIZE];
		slavefds[i] = -1;
	}

	int serverfd = open_server(PORT);
	if(serverfd < 0)
	{
		cout<<"openning server failed"<<endl;
		return 1;
	}

	struct sockaddr_in slaveaddr;
	int addrlen = sizeof(slaveaddr);
	char *haddrp;

	while(1)
	{
		slavefds[conn_num_slave] = accept(serverfd, (struct sockaddr *) &slaveaddr, (socklen_t *) &addrlen);
		if( slavefds[conn_num_slave] < 0)
		{
			cout<<"accepting failed"<<endl;
			continue;
		}
		else
		{
			// add the file descriptor to the fdslist and update maxfd
			FD_SET(slavefds[conn_num_slave], &fdslist);
			if(slavefds[conn_num_slave]>maxfd)
				maxfd = slavefds[conn_num_slave];
			// connection from slave succeeded
			conn_num_slave++;

			// get ip address of slave
			haddrp = inet_ntoa(slaveaddr.sin_addr);
			printf("connection from %s \n", haddrp);

			if(conn_num_slave==num_slave)
			{
				cout<<"all slave nodes are connected successfully"<<endl;
				break;
			}
			else if(conn_num_slave>num_slave)
				cout<<"number of slave connection exceeded allowed limits"<<endl;
		}
		// sleeps for 0.01 seconds. change this if necessary
		usleep(10000);
	}

	// set sockets to be non-blocking socket
	fcntl(serverfd, F_SETFL, O_NONBLOCK);
	for(int i=0;i<num_slave;i++)
		fcntl(slavefds[i], F_SETFL, O_NONBLOCK);

	// send the message to slave for debugging
	for(int i=0;i<num_slave;i++)
		write(slavefds[i], "hello slaves", BUF_SIZE);
	// get signal from slaves through select()
	int readbytes = 0;
	int closed = 0;
	while(1)
	{
		selectfds = fdslist;
		if(select(maxfd+1, &selectfds, NULL, NULL, NULL)<0)
			cout<<"select error"<<endl;
		for(int i=0; i<num_slave; i++)
		{
			if(FD_ISSET(slavefds[i], &selectfds))
			{
				readbytes = read(slavefds[i], read_bufs[i], BUF_SIZE);
				if(readbytes == 0)
				{
					// connection closed
					if(close(slavefds[i])<0)
						cout<<"closing slave "<<i<<" socket failed"<<endl;
					else
					{
						// count closed slaves and remove file descriptor from the list
						cout<<"slave "<<i<<" closed"<<endl;
						FD_CLR(slavefds[i], &fdslist);
						closed++;
					}
				}
				else
				{
					// signal from the slave
					// deal with each signal
					cout<<"signal from slave "<<i<<": "<<read_bufs[i]<<endl;

				}
			}
		}
		// break if all slaves are closed
		if(closed == num_slave)
			break;

		// sleeps for 0.01 seconds. change this if necessary
		//usleep(10000);
		usleep(5000000);
	}


	// close master socket
	if(close(serverfd)<0)
		cout<<"closing socket failed"<<endl;
	else
		cout<<"master closed"<<endl;

	// delete all dynamically allocated data
	delete [] slavefds;
	for(int i=0;i<num_slave;i++)
		delete read_bufs[i];
	delete [] read_bufs;

	cout<<"exiting master..."<<endl;

	return 0;
}
