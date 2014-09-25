#include <iostream>
#include <vector>
#include <fstream>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <mapreduce/definitions.hh>
#include "cacheclient.hh"
#include "histogram.hh"

using namespace std;

// this process should be run on master node
// the role of this process is to manage the cache boundaries

int dhtport = -1;

int serverfd = -1;
int ipcfd = -1;

int buffersize = 8388608; // 8 MB buffer

vector<string> nodelist;
vector<cacheclient*> clients;

histogram* thehistogram;

char read_buf[BUF_SIZE]; // read buffer for signal_listener thread
char write_buf[BUF_SIZE]; // write buffer for signal_listener thread

void open_server(int port);

int main(int argc, char** argv)
{
	// initialize data structures from setup.conf
	ifstream conf;
	string token;
	string confpath = LIB_PATH;
	confpath.append("setup.conf");
	conf.open(confpath.c_str());

	master_connection themaster; // from <orthrus/cacheclient.hh>

	conf>>token;
	while(!conf.eof())
	{
		if(token == "port")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "dhtport")
		{
			conf>>token;
			dhtport = atoi(token.c_str());
		}
		else if(token == "max_job")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "master_address")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else
		{
			cout<<"[cacheserver]Unknown configure record: "<<token<<endl;
		}
		conf>>token;
	}
	conf.close();

	// read the node list information
	ifstream nodelistfile;
	string filepath = LIB_PATH;
	filepath.append("nodelist.conf");

	nodelistfile.open(filepath.c_str());
	nodelistfile>>token;

	while(!nodelistfile.eof())
	{
		nodelist.push_back(token);
		nodelistfile>>token;
	}

	if(access(IPC_PATH, F_OK) == 0)
	{
		unlink(IPC_PATH);
	}

	open_server(dhtport);
	if(serverfd < 0)
	{
		cout<<"[cacheserver]\033[0;31mOpenning server failed\033[0m"<<endl;
		return 1;
	}

	struct sockaddr_in connaddr;
	int addrlen = sizeof(connaddr);
	char* haddrp;

	while(clients.size() < nodelist.size())
	{
		int fd;
		fd = accept(serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);

		if(fd < 0)
		{
			cout<<"[cacheserver]Accepting failed"<<endl;

			// sleep 1 milli second. change this if necessary
			// usleep(1000);
			continue;
		}
		else if(fd == 0)
		{
			cout<<"[cacheserver]Accepting failed"<<endl;
			exit(1);
		}
		else
		{
			// get ip address of client
			haddrp = inet_ntoa(connaddr.sin_addr);

			// add connected client to the vector
			clients.push_back(new cacheclient(fd, haddrp));

			// set socket to be non-blocking socket to avoid deadlock
			fcntl(fd, F_SETFL, O_NONBLOCK);
		}
	}

	// receive master connection
	int tmpfd = accept(ipcfd, NULL, NULL);
	if(tmpfd > 0) // master is connected
	{
		int valid = 1;
		setsockopt(tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
		setsockopt(tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));
		setsockopt(tmpfd, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(valid));
	}
	else
	{
		cout<<"[cacheserver]Connection from master is unsuccessful"<<endl;
		exit(-1);
	}

	// set fd of master
	themaster.set_fd(tmpfd);

	// set the server fd as nonblocking mode
	fcntl(serverfd, F_SETFL, O_NONBLOCK);
	fcntl(tmpfd, F_SETFL, O_NONBLOCK);

	// initialize the EM-KDE histogram
	thehistogram = new histogram(nodelist.size(), NUMBIN);

	// a main iteration loop
	int readbytes = -1;
	int fd;


	struct timeval time_start;
	struct timeval time_end;

	gettimeofday(&time_start, NULL);
	gettimeofday(&time_end, NULL);

	while(1)
	{
		fd = accept(serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);

		if(fd > 0) // a client is connected. which will send stop message
		{
			while(readbytes < 0)
				readbytes = nbread(fd, read_buf);

			if(readbytes == 0)
			{
				cout<<"[cacheserver]Connection abnormally closed from client"<<endl;
			}
			else // a message
			{
				if(strncmp(read_buf, "stop", 4) == 0)
				{
					for(int i = 0; (unsigned)i < clients.size(); i++)
					{
						close(clients[i]->get_fd());
					}
					close(serverfd);
					return 0;
				}
				else // message other than "stop"
				{
					cout<<"[cacheserver]Unexpected message from client"<<read_buf<<endl;
				}
			}
		}


		// listen to master
		readbytes = nbread(themaster.get_fd(), read_buf);

		if(readbytes == 0)
		{
			cout<<"[cacheserver]Connection abnormally closed from master(ipc)"<<endl;
			usleep(10000); // 10 msec
		}
		else if(readbytes > 0) // a message accepted
		{
			if(strncmp(read_buf, "boundaries", 10) == 0)
			{
				/*
				string token;
				double doubletoken;
				stringstream ss;

				// read boundary information and parse it to update the boundary
				ss << read_buf;

				ss >> token; // boundaries

				for(int i = 0; i < nodelist.size(); i++)
				{
					ss >> doubletoken;
					thehistogram->set_boundary(i, doubletoken);
				}
				*/

				// distribute the updated boundaries to each nodes 
				for(int i = 0; (unsigned)i < clients.size(); i++)
				{
					nbwrite(clients[i]->get_fd(), read_buf);
				}
				

			}
			else // unknown message
			{
				cout<<"[cacheserver]Unknown message from master node";
			}
		}



		/*
		for(int i = 0; (unsigned)i < clients.size(); i++)
		{
			// do nothing currently
		}
		*/

		// sleeps for 1 millisecond
		// usleep(1000);
	}

	return 0;
}

void open_server(int port)
{
	struct sockaddr_in serveraddr;

	// socket open
	serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if(serverfd < 0)
		cout<<"[cacheserver]Socket opening failed"<<endl;
	
	int valid = 1;
	setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(valid));

	// bind
	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) port);

	if(bind(serverfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		cout<<"[cacheserver]\033[0;31mBinding failed\033[0m"<<endl;
	}

	// listen
	if(listen(serverfd, BACKLOG) < 0)
	{
		cout<<"[cacheserver]Listening failed"<<endl;
	}

	// prepare AF_UNIX socket for the master_connection
	struct sockaddr_un serveraddr2;
	ipcfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if(ipcfd < 0)
	{
		cout<<"[cacheserver]AF_UNIX socket openning failed"<<endl;
		exit(-1);
	}

	// bind
	memset((void*) &serveraddr2, 0, sizeof(serveraddr2));
	serveraddr2.sun_family = AF_UNIX;
	strcpy(serveraddr2.sun_path, IPC_PATH);

	if(bind(ipcfd, (struct sockaddr *)&serveraddr2, SUN_LEN(&serveraddr2)) < 0)
	{
		cout<<"[cacheserver]IPC Binding fialed"<<endl;
		exit(-1);
	}

	// listen
	if(listen(ipcfd, BACKLOG) < 0)
	{
		cout<<"[cacheserver]Listening failed"<<endl;
		exit(-1);
	}
}
