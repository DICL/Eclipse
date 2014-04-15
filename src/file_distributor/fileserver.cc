#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <mapreduce/definitions.hh>
#include "fileserver.hh"

using namespace std;

char master_address[BUF_SIZE];

bool master_is_set = false;

int port = -1;
int dhtport = -1;

fileserver afileserver;

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
		if(token == "dhtport")
		{
			conf>>token;
			dhtport = atoi(token.c_str());
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
		else if(token == "master_address")
		{
			conf>>token;
			strcpy(master_address, token.c_str());
			master_is_set = true;
		}
		else
		{
			cout<<"[slave]Unknown configure record: "<<token<<endl;
		}
		conf>>token;
	}
	conf.close();

	// verify initialization
	if(port == -1)
	{
		cout<<"[slave]port should be specified in the setup.conf"<<endl;
		return 1;
	}
	if(master_is_set == false)
	{
		cout<<"[slave]master_address should be specified in the setup.conf"<<endl;
		return 1;
	}

	// run the file server
	afileserver.run_server(dhtport);

	// TODO: check whether connection to master node is needed
	return 0;
}
