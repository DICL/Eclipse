#ifndef __FILECLIENT__
#define __FILECLIENT__

#include <iostream>
#include <set>
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

public:
	msgaggregator Iwritebuffer; // a buffer for the Iwrite
	msgaggregator Owritebuffer; // a buffer for the Owrite

	fileclient();
	~fileclient();
	bool write_record(string filename, string data, datatype atype); // append mode, write a record
	void close_server(); // this function is used to notify the server that writing is done
	void wait_write(); // wait until write is done
	bool read_request(string req, datatype atype, msgaggregator* keybuf, set<string>* keys); // connect to read file
	bool read_record(string& record); // read sentences from connected file(after read_request())
	int connect_to_server(int writeid); // returns fd of file server
	void configure_buffer_initial(string jobdirpath, string appname, string inputfilepath, bool isIcache); // set the initial string of the Iwritebuffer
};

fileclient::fileclient()
{
	serverfd = -1;
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
	close(serverfd);
	serverfd = -1;
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

	serverfd = fd;

	// buffer for Iwrite and Owrite
	Iwritebuffer.set_fd(fd);
	Owritebuffer.set_fd(fd);
	Owritebuffer.configure_initial("Owrite\n");

	// send the write id to the file server
	stringstream ss;
	string message;
	ss << "Wid ";
	ss << writeid;
	message = ss.str();
	memset(write_buf, 0, BUF_SIZE);
	strcpy(write_buf, message.c_str());
	nbwrite(serverfd, write_buf);

	return fd;
}

void fileclient::configure_buffer_initial(string jobdirpath, string appname, string inputfilepath, bool isIcache)
{
	// buffer for Iwrite
	string initial;
	if(isIcache)
	{
		initial = "ICwrite ";
		initial.append(jobdirpath);
		initial.append(" ");
		initial.append(appname);
		initial.append(" ");
		initial.append(inputfilepath);
		initial.append("\n");
	}
	else
	{
		initial = "Iwrite ";
		initial.append(jobdirpath);
		initial.append(" ");
		initial.append(inputfilepath);
		initial.append("\n");
	}

	Iwritebuffer.flush();
	Iwritebuffer.configure_initial(initial);
}

// close should be done exclusively with close_server() function
bool fileclient::write_record(string filename, string data, datatype atype)
{
	if(atype == INTERMEDIATE)
	{
		// generate request string
		string str = filename;
		str.append(" ");
		str.append(data);
		Iwritebuffer.add_record(str);
	}
	else if(atype == OUTPUT)// atype == OUTPUT
	{
		// generate request string
		string str = filename;
		str.append(" ");
		str.append(data);
		Owritebuffer.add_record(str);
	}
	else // atype == RAW
	{
		cout<<"[fileclient]Wrong data type for write_record()"<<endl;
	}

//cout<<"\033[0;33m\trecord sent from client: \033[0m"<<write_buf<<endl;
	return true;
}

void fileclient::close_server()
{
	close(serverfd);
}

void fileclient::wait_write() // wait until write is done
{
	// flush the write buffer
	Iwritebuffer.flush();
	Owritebuffer.flush();

	// generate request string
	string str = "Wwrite"; // waiting message

	// send the message to the fileserver
	memset(write_buf, 0, BUF_SIZE);
	strcpy(write_buf, str.c_str());
	nbwrite(serverfd, write_buf);

	int read_bytes;
	while(1)
	{
		read_bytes = read(serverfd, read_buf, BUF_CUT);  // use BUF_CUT as read size exceptively
		if(read_bytes > 0)
		{
			cout<<"[fileclient]Unexpected message while waiting close of socket during the write: "<<read_buf<<endl;
		}
		else if(read_bytes == 0)
		{
			break;
		}

		// sleeps for 1 milli second
		usleep(1000);
	}
	close(serverfd);
	return;
}

bool fileclient::read_request(string request, datatype atype, msgaggregator* keybuffer, set<string>* reported_keys)
{
	// generate request string to send file server
	string str;
	if(atype == RAW)
	{
		str = "Rread ";
		str.append(request);

		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, str.c_str());

		nbwrite(serverfd, write_buf);

		// return true if 1 is returned, false if 0 is returned
		int readbytes;
		while(1)
		{
			readbytes = nbread(serverfd, read_buf); 

			if(readbytes == 0) // connection closed abnormally
			{
				close_server();

				cout<<"[fileclient]Connection abnormally closed"<<endl;

				return false;
			}
			else if(readbytes < 0)
			{
				continue;
			}
			else
			{
				if(read_buf[0] == 1) // intermediate cache hit
				{
					while(1)
					{
						readbytes = nbread(serverfd, read_buf);

						if(readbytes == 0) // connection closed abnormally
						{
							close_server();

							cout<<"[fileclient]Connection closed abnormally"<<endl;

							return false;
						}
						else if(readbytes < 0)
						{
							// sleeps for 1 millisecond
							usleep(1000);
							continue;
						}
						else
						{
							if(read_buf[0] == 1) // distributing intermediate data finished
							{
								return true;
							}
							else // key arrived
							{
//cout<<"[fileclient]Ikey arrived to the file client: "<<read_buf<<endl;
								// accept key and insert it to key buffer if its not in the list
								char* token = strtok(read_buf, "\n"); // <- "Ikey"
								token = strtok(NULL, "\n");
								while(token != NULL)
								{
									if(reported_keys->find(token) == reported_keys->end())
									{
										reported_keys->insert(token);
										keybuffer->add_record(token);
									}
									token = strtok(NULL, "\n");
								}
							}
						}
					}
					return true;
				}
				else if(read_buf[0] == 0) // intermediate cache miss
				{
					return false;
				}
			}
		}
		return true;
	}
	else if(atype == INTERMEDIATE)
	{
		str = "Iread ";
		str.append(request);

		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, str.c_str());

		nbwrite(serverfd, write_buf);

		return false;
	}
	else // atype <- OUTPUT
	{
		cout<<"[fileclient]Unexpected datatype in the read_request()"<<endl;
	}

	return false;
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
			readbytes = nbread(serverfd, read_buf);
			if(readbytes == 0) // connection closed abnormally
			{
				cout<<"[fileclient]Connection abnormally closed"<<endl;
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
				if(read_buf[0] == -1) // the read stream is finished(Eread)
				{
//cout<<"\t\tEnd of read stream at client"<<endl;
					record = "";
					return false;
				}
				else
				{
					token = strtok(read_buf, "\n");
					if(token == NULL)
					{
						continue;
					}
					else
					{
						ptr = read_buf + strlen(token) + 1;
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
