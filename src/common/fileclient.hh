#ifndef __FILECLIENT__
#define __FILECLIENT__

#include <iostream>
#include <pthread.h>
#include <fcntl.h>
#include <mapreduce/definitions.hh>

using namespace std;

class fileclient // each task process will has an object of fileclient
{
private:
	int serverfd;
	char read_buf[BUF_SIZE];
	char write_buf[BUF_SIZE];
	int connect_to_server(string address, int port); // returns fd of file server

public:
	fileclient();
	~fileclient();
	bool write_attach(string address, int port, string filename); // connect to write file
	bool write_record(string data); // append mode, write a sentence
	int close_server(); // this function is used to notify the server that writing is done
	bool read_attach(string address, int port, string filename); // connect to read file
	bool read_record(string* record); // read sentences from connected file(after read_attach())
};

fileclient::fileclient()
{
	this->serverfd = -1;
}

fileclient::~fileclient()
{
	close(this->serverfd);
	this->serverfd = -1;
}

int fileclient::connect_to_server(string address, int port)
{
	int fd;
	struct sockaddr_in serveraddr;
	struct hostent* hp;

	// SOCK_STREAM -> tcp
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
	{
		cout<<"[slave]Openning socket failed"<<endl;
		exit(1);
	}

	hp = gethostbyname(address.c_str());

	if (hp == NULL)
		cout<<"[fileclient]Cannot find host by host name"<<endl;

	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	if(connect(fd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
		cout<<"[fileclient]Cannot connect to the file server."<<endl;
	return fd;
}

bool fileclient::write_attach(string address, int port, string filename)
{
	// generate request string to send file server
	string str = "write ";
	str.append(filename);
	
	memset(this->write_buf, 0, BUF_SIZE);
	strcpy(this->write_buf, str.c_str());
	connect_to_server(address, port);
	nbwrite(this->serverfd, this->write_buf);
	return true;
}

// close should be done exclusively with close_server() function
bool fileclient::write_record(string data)
{
	memset(this->write_buf, 0, BUF_SIZE);
	strcpy(this->write_buf, data.c_str());
	nbwrite(this->serverfd, this->write_buf);
	return true;
}

int fileclient::close_server()
{
	close(this->serverfd);
	this->serverfd = -1;
}

bool fileclient::read_attach(string address, int port, string filename)
{
	// generate request string to send file server
	string str = "read ";
	str.append(filename);

	memset(this->write_buf, 0, BUF_SIZE);
	strcpy(this->write_buf, str.c_str());
	connect_to_server(address, port);
	nbwrite(this->serverfd, this->write_buf);
	return true;
}

bool fileclient::read_record(string* record) // read through the pipe with blocking way
{
	int readbytes;
	while(1)
	{
		readbytes = nbread(this->serverfd, this->read_buf);
		if(readbytes == 0) // when reaches end of file
		{
			close_server();

			// return empty string
			*record = "";
			return false;
		}
		else if(readbytes < 0)
		{
			continue;
		}
		else // successful read
		{
			*record = this->read_buf;
			return true;
		}
	}
	return false;
}

#endif
