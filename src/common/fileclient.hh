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
#include <common/msgaggregator.hh>
#include <mapreduce/definitions.hh>

using namespace std;

class fileclient // each task process will have two objects(read/write) of fileclient
{
private:
	int serverfd;
	char* token;
	char* ptr;
	char read_buf[BUF_SIZE];
	char write_buf[BUF_SIZE];
	//writebuffer* wbuffer;

public:
	msgaggregator writebuffer; // a buffer for the write

	fileclient();
	~fileclient();
	bool write_record(string filename, string data); // append mode, write a record
	void close_server(); // this function is used to notify the server that writing is done
	void wait_write(); // wait until write is done
	bool read_request(string filename, datatype atype); // connect to read file
	bool read_record(string& record); // read sentences from connected file(after read_request())
	int connect_to_server(int writeid); // returns fd of file server
};

fileclient::fileclient()
{
	this->serverfd = -1;
	token = NULL;
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

int fileclient::connect_to_server(int writeid)
{
	int fd;
	int buffersize = 8388608; // 8 MB buffer
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
	}

	// set socket to be nonblocking
	fcntl(fd, F_SETFL, O_NONBLOCK);
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));

//gettimeofday(&time_end, NULL);
//elapsed = 1000000.0*(time_end.tv_sec - time_start.tv_sec);
//elapsed += (time_end.tv_usec - time_start.tv_usec);
//elapsed /= 1000.0;
//if(elapsed > 10.0)
//cout<<"\033[0;33m\tconnect() elapsed: "<<elapsed<<" milli seconds\033[0m"<<endl;


	this->serverfd = fd;

	// buffer for write
	writebuffer.set_fd(fd);
	writebuffer.configure_initial("write\n");

	// send the write id to the file server
	stringstream ss;
	string message;
	ss << "Wid ";
	ss << writeid;
	message = ss.str();
	memset(write_buf, 0, BUF_SIZE);
	strcpy(write_buf, message.c_str());
	nbwrite(this->serverfd, write_buf);

	return fd;
}

// close should be done exclusively with close_server() function
bool fileclient::write_record(string filename, string data)
{
/*
	// generate request string to send file server
	string str = "write ";

	str.append(filename);
	str.append(" ");
	str.append(data);

	// send the message to the server
	memset(this->write_buf, 0, BUF_SIZE);
	strcpy(this->write_buf, str.c_str());
	nbwrite(this->serverfd, this->write_buf);
*/
	// generate request string
	string str = filename;
	str.append(" ");
	str.append(data);
	writebuffer.add_record(str);

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
	// flush the write buffer
	writebuffer.flush();

	// generate request string
	string str = "Wwrite"; // waiting message

	// send the message to the fileserver
	memset(write_buf, 0, BUF_SIZE);
	strcpy(write_buf, str.c_str());
	nbwrite(this->serverfd, write_buf);

	int read_bytes;
	while(1)
	{
		read_bytes = read(this->serverfd, this->read_buf, BUF_CUT);  // use BUF_CUT as read size exceptively
		if(read_bytes > 0)
		{
			cout<<"[fileclient]Unexpected message while waiting close of socket during the write: "<<this->read_buf<<endl;
		}
		else if(read_bytes == 0)
		{
			break;
		}
		// sleeps for 1 milli second
		usleep(1000);
	}
	close(this->serverfd);
	return;
}

bool fileclient::read_request(string filename, datatype atype)
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
	else // atype <- OUTPUT
	{
		cout<<"[fileclient]Unexpected datatype in the read_request()"<<endl;
	}
	str.append(filename);

	memset(this->write_buf, 0, BUF_SIZE);
	strcpy(this->write_buf, str.c_str());

	nbwrite(this->serverfd, this->write_buf);
	return true;
}

bool fileclient::read_record(string& record) // read through the socket with blocking way and token each record
{
	if(token != NULL)
	{
		token = strtok(ptr, "\n");
	}

	if(token == NULL)
	{
		int readbytes;
		while(1)
		{
			readbytes = nbread(this->serverfd, this->read_buf);
			if(readbytes == 0) // when reaches end of file
			{
				close_server();

				// return empty string
				record = "";
				return false;
			}
			else if(readbytes < 0)
			{
				continue;
			}
			else // successful read
			{
//cout<<"\tread stream at client: "<<read_buf<<endl;
//cout<<"\tread stream bytes: "<<readbytes<<endl;
				if(this->read_buf[0] == -1) // the read stream is finished(Eread)
				{
//cout<<"\t\tEnd of read stream at client"<<endl;
					record = "";
					return false;
				}
				else
				{
					token = strtok(this->read_buf, "\n");
					if(token == NULL)
					{
						continue;
					}
					else
					{
						ptr = this->read_buf + strlen(token) + 1;
						record = token;
//cout<<"read stream at client side: "<<record<<endl;
						return true;
					}
				}
//cout<<"\033[0;32m\trecord received in client: \033[0m"<<*record<<endl;
//cout<<"\033[0;32m\treadbytes: \033[0m"<<readbytes<<endl;
			}
		}
		return false;
	}
	else
	{
		record = token;
		ptr = token + strlen(token) + 1;
//cout<<"read stream at client side: "<<record<<endl;
		return true;
	}
}

#endif
