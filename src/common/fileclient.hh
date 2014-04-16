#ifndef __FILECLIENT__
#define __FILECLIENT__

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <mapreduce/definitions.hh>

using namespace std;

class fileclient // each task process will has an object of fileclient
{
private:
	int serverfd;
	char read_buf[BUF_SIZE];
	char write_buf[BUF_SIZE];
	int connect_to_server(); // returns fd of file server

public:
	fileclient();
	~fileclient();
	bool write_record(string filename, string data, datatype atype); // append mode, write a record
	void close_server(); // this function is used to notify the server that writing is done
	void wait_write(); // wait until write is done
	bool read_attach(string filename, datatype atype); // connect to read file
	bool read_record(string* record); // read sentences from connected file(after read_attach())
};

fileclient::fileclient()
{
	this->serverfd = -1;
}

fileclient::~fileclient()
{
	/*
	if(this->serverfd != -1)
	{
		while(close(this->serverfd) < 0)
		{
			cout<<"[fileclient]close failed"<<endl;

			// sleep for 1 millisecond
			usleep(1000);
		}
	}
	*/
	close(this->serverfd);
	this->serverfd = -1;
}

int fileclient::connect_to_server()
{
	int fd;
	struct sockaddr_un serveraddr;

	// SOCK_STREAM -> tcp
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0)
	{
		cout<<"[fileclient]Openning socket failed"<<endl;
		exit(1);
	}

	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sun_family = AF_UNIX;
	strcpy(serveraddr.sun_path, IPC_PATH);

//struct timeval time_start;
//struct timeval time_end;
//double elapsed = 0.0;
//gettimeofday(&time_start, NULL);

	while(connect(fd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		// sleep for 1 miilisecond
		usleep(1000);
		//cout<<"[fileclient]The connection failed to the file server"<<endl;
		//exit(1);
	}

	// set socket to be nonblocking
	fcntl(fd, F_SETFL, O_NONBLOCK);

//gettimeofday(&time_end, NULL);
//elapsed = 1000000.0*(time_end.tv_sec - time_start.tv_sec);
//elapsed += (time_end.tv_usec - time_start.tv_usec);
//elapsed /= 1000.0;
//if(elapsed > 10.0)
//cout<<"\033[0;33m\tconnect() elapsed: "<<elapsed<<" milli seconds\033[0m"<<endl;


//cout<<"the connect is straggler"<<endl;
	return fd;
}

// close should be done exclusively with close_server() function
bool fileclient::write_record(string filename, string data, datatype atype)
{
	// connect to the fileserver
	this->serverfd = connect_to_server();

	// generate request string to send file server
	string str;
	if(atype == INTERMEDIATE)
	{
		str = "Iwrite ";
	}
	else if(atype == OUTPUT)
	{
		str = "Owrite ";
	}
	else // atype <- OUTPUT
	{
		cout<<"[fileclient]An invalid output type"<<endl;
		return false;
	}
	str.append(filename);
	str.append(" ");
	str.append(data);

	// send the message to the server
	memset(this->write_buf, 0, BUF_SIZE);
	strcpy(this->write_buf, str.c_str());
	nbwrite(this->serverfd, this->write_buf);

//cout<<"\033[0;33m\trecord sent from client: \033[0m"<<write_buf<<endl;
	return true;
}

void fileclient::close_server()
{
/*
	if(this->serverfd != -1)
	{
		while(close(this->serverfd) < 0)
		{
			cout<<"[fileclient]close failed"<<endl;

			// sleep for 1 millisecond
			usleep(1000);
		}
	}
	this->serverfd = -1;
*/
	close(this->serverfd);
}

void fileclient::wait_write() // wait until write is done
{
	int read_bytes;
	while(1)
	{
		read_bytes = read(this->serverfd, read_buf, BUF_CUT);  // use BUF_CUT as read size exceptively
		if(read_bytes > 0)
		{
			cout<<"[fileclient]Unexpected message while waiting close of socket during the write"<<endl;
		}
		else if(read_bytes == 0)
		{
			break;
		}
	}
	close(this->serverfd);
	return;
}

bool fileclient::read_attach(string filename, datatype atype)
{
	// generate request string to send file server
	string str;
	if(atype == RAW)
	{
		str = "Rread ";
	}
	else if(atype == INTERMEDIATE)
	{
		str = "Iread ";
	}
	else
	{
		cout<<"[fileclient]Invalid read data type"<<endl;
		return false;
	}
	str.append(filename);

	memset(this->write_buf, 0, BUF_SIZE);
	strcpy(this->write_buf, str.c_str());

	this->serverfd = connect_to_server();
	nbwrite(this->serverfd, this->write_buf);
	return true;
}

bool fileclient::read_record(string* record) // read through the socket with blocking way
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
//cout<<"\033[0;32m\tconnection is closed in read_record\033[0m"<<endl;
			return false;
		}
		else if(readbytes < 0)
		{
			continue;
		}
		else // successful read
		{
			*record = this->read_buf;
//cout<<"\033[0;32m\trecord received in client: \033[0m"<<*record<<endl;
//cout<<"\033[0;32m\treadbytes: \033[0m"<<readbytes<<endl;
			return true;
		}
	}
	return false;
}

#endif
