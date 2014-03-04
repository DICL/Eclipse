#ifndef __FILESERVER__
#define __FILESERVER__

#include <iostream>
#include <vector>
#include <mapreduce/definitions.hh>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include "file_connclient.hh"
#include <sys/fcntl.h>

using namespace std;

class fileserver // each slave node has an object of fileserver
{
private:
	int serverfd;
	vector<file_connclient*> clients;
	char read_buf[BUF_SIZE];
	char write_buf[BUF_SIZE];

public:
	fileserver();
	int run_server(int port);
};

fileserver::fileserver()
{
	this->serverfd = -1;
}

int fileserver::run_server(int port)
{
	int fd;
	struct sockaddr_in serveraddr;

	// socket open
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
	{
		cout<<"[fileserver]Socket opening failed"<<endl;
		return -1;
	}
	else
	{
		this->serverfd = fd;
	}
	
	// bind
	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) port);

	if(bind(fd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		cout<<"[fileserver]\033[0;31mBinding failed\033[0m"<<endl;
		return -1;
	}

	// listen
	if(listen(fd, BACKLOG) < 0)
	{
		cout<<"[master]Listening failed"<<endl;
		return -1;
	}
	
	// set the server fd as nonblocking mode
	fcntl(fd, F_SETFL, O_NONBLOCK);

	int tmpfd = -1;
	struct sockaddr_in connaddr;
	int addrlen = sizeof(connaddr);

	// listen connections and signals from clients
	while(1)
	{
		tmpfd = accept(serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		if(tmpfd > 0) // new file client is connected
		{
			char* token;
			file_client_role inputrole = UNDEFINED;
			string filename;

			nbread(tmpfd, read_buf);

			token = strtok(read_buf, " "); // <- read or write

			if(strncmp(token, "read", 4) == 0)
			{
				inputrole = READ;
			}
			else if(strncmp(token, "write", 5) == 0)
			{
				inputrole = WRITE;
			}

			token = strtok(NULL, " "); // <- file name
			filename = token;

			// create new clients
			this->clients.push_back(new file_connclient(tmpfd, inputrole, filename));

			if(clients.back()->get_role() == READ)
				clients.back()->open_readfile(filename);
			else if(clients.back()->get_role() == WRITE)
				clients.back()->open_writefile(filename);
			else
				cout<<"[fileserver]The role is not defined"<<endl;

			// set socket to be non-blocking socket to avoid deadlock
			fcntl(tmpfd, F_SETFL, O_NONBLOCK);
		}
		for(int i=0;(unsigned)i<this->clients.size();i++)
		{
			if(clients[i]->get_role() == UNDEFINED)
			{
				cout<<"[fileserver]The role is not defined for a clients"<<endl;
			}
			else if(clients[i]->get_role() == READ)
			{
				// read file and transfer it to client.
				// close the connection and delete client from vector after all record is transferred.
				if(clients[i]->get_remain() == 0) // if next record should be read
				{
					string record;
					if(clients[i]->read_record(&record))
					{
						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, record.c_str());


						// prepare the write to remote client
						clients[i]->prep_send(write_buf);

						// send a record
						clients[i]->send_record();
						//cout<<"\033[0;32mrecord read from server: \033[0m"<<write_buf<<endl;


						//cout<<"\033[0;32mrecord sent from server: \033[0m"<<write_buf<<endl;
					}
					else // if all record is transferred
					{
						// close the fd to notify that all records are transferred.
						close(clients[i]->get_fd());

						// delete the client from the vector
						delete clients[i];
						clients.erase(clients.begin()+i);
						i--;
					}
				}
				else
				{
					clients[i]->send_record();
				}
			}
			else // WRITE role
			{
				// read the contents of the file from client until the pipe is closed
				string record;
				int readbytes;
				
				readbytes = nbread(clients[i]->get_fd(), read_buf);
				if(readbytes == 0)
				{
					close(clients[i]->get_fd());

					// delete the client from the vector
					delete clients[i];
					clients.erase(clients.begin()+i);
				}
				else if(readbytes > 0)
				{
//cout<<"\033[0;33mrecord received in server: \033[0m"<<read_buf<<endl;
					record = read_buf;
					clients[i]->write_record(record, write_buf);
//cout<<"\033[0;33mrecord written in server: \033[0m"<<read_buf<<endl;
				}
				else // if there is no message from the client
				{
					// do nothing as dafault
					continue;
				}
			}
		}
	}
	return 0;
}

#endif
