#include <iostream>
#include <vector>
#include <fstream>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
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

vector<string> nodelist;
vector<cacheclient*> clients;

histogram* thehistogram;

char read_buf[BUF_SIZE]; // read buffer for signal_listener thread
char write_buf[BUF_SIZE]; // write buffer for signal_listener thread

int open_server(int port);

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

	int serverfd = open_server(dhtport);
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

	// initialize the histogram
	thehistogram = new histogram(nodelist.size());

	// a main iteration loop
	while(1)
	{
		if(1) // if 
		{
			for(int i = 0; (unsigned)i < clients.size(); i++)
			{
			}

sleep(10);
		}
	}

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
	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) port);

	if(bind(serverfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		cout<<"[master]\033[0;31mBinding failed\033[0m"<<endl;
		return -1;
	}

	// listen
	if(listen(serverfd, BACKLOG) < 0)
	{
		cout<<"[master]Listening failed"<<endl;
		return -1;
	}

	return serverfd;
}
