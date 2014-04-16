#ifndef __FILESERVER__
#define __FILESERVER__

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <mapreduce/definitions.hh>
#include <common/hash.hh>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include "file_connclient.hh"
#include "messagebuffer.hh"
#include "filepeer.hh"
#include "filebridge.hh"
#include <sys/fcntl.h>

using namespace std;

class fileserver // each slave node has an object of fileserver
{
	private:
		int serverfd;
		int ipcfd;
		int fbidclock;
		string localhostname;
		vector<string> nodelist;
		vector<filepeer*> peers;
		vector<file_connclient*> clients;
		vector<filebridge*> bridges;

		char read_buf[BUF_SIZE];
		char write_buf[BUF_SIZE];

	public:
		fileserver();
		filepeer* find_peer(string address);
		filebridge* find_bridge(int id);
		int run_server(int port);
};

fileserver::fileserver()
{
	this->serverfd = -1;
	this->ipcfd = -1;
	this->fbidclock = 0; // fb id starts from 0
}

int fileserver::run_server(int port)
{
	// read hostname from hostname file
	ifstream hostfile;
	string token;
	string hostpath = DHT_PATH;
	hostpath.append("hostname");
	hostfile.open(hostpath.c_str());
	hostfile>>localhostname;

	hostfile.close();

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
	nodelistfile.close();

	if (access(IPC_PATH, F_OK) == 0)
	{
		unlink(IPC_PATH);
	}

	// determine the network topology by reading node list information
	int networkidx = -1;
	for(int i = 0; (unsigned) i < nodelist.size(); i++)
	{
		if(nodelist[i] == localhostname)
		{
			networkidx = i;
			break;
		}
	}

	// listen connections from peers and complete the eclipse network
	for(int i = 0; i < networkidx; i++)
	{
		int clientfd = -1;
		struct sockaddr_in serveraddr;
		struct hostent *hp;

		// SOCK_STREAM -> tcp
		clientfd = socket(AF_INET, SOCK_STREAM, 0);
		if(clientfd < 0)
		{
			cout<<"[fileserver]Openning socket failed"<<endl;
			exit(1);
		}

		hp = gethostbyname(nodelist[i].c_str());

		if(hp == NULL)
		{
			cout<<"[fileserver]Cannot find host by host name"<<endl;
			return -1;
		}

		memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
		serveraddr.sin_family = AF_INET;
		memcpy(&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
		serveraddr.sin_port = htons(port);

		if(connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
		{
			//cout<<"[fileserver]Cannot connect. Retrying..."<<endl;
			//cout<<"\thost name: "<<nodelist[i]<<endl;
			i--;
			continue;
		}

		// register the file peer
		peers.push_back(new filepeer(clientfd, nodelist[i]));

		// set the peer fd as nonblocking mode
		fcntl(clientfd, F_SETFL, O_NONBLOCK);
	}

	// socket open for listen
	int fd;
	int tmpfd = -1;
	struct sockaddr_in serveraddr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
	{
		cout<<"[fileserver]Socket opening failed"<<endl;
		exit(-1);
	}

	// bind
	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) port);

	if(bind(fd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		cout<<"[fileserver]\033[0;31mBinding failed\033[0m"<<endl;
		exit(-1);
	}

	// listen
	if(listen(fd, BACKLOG) < 0)
	{
		cout<<"[master]Listening failed"<<endl;
		exit(-1);
		return -1;
	}

	// connect to other peer eclipse nodes
	for(int i = networkidx + 1; (unsigned) i < nodelist.size(); i++)
	{
		struct sockaddr_in connaddr;
		int addrlen = sizeof(connaddr);

		tmpfd = accept(fd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		if(tmpfd > 0)
		{
			char* haddrp = inet_ntoa(connaddr.sin_addr);

			peers.push_back(new filepeer(tmpfd, haddrp));

			// set the peer fd as nonblocking mode
			fcntl(tmpfd, F_SETFL, O_NONBLOCK);
		}
		else // connection failed
		{
			// retry with same index
			i--;
			continue;
		}
	}

	// register the server fd
	serverfd = fd;

	cout<<"[fileserver]Eclipse network successfully established(id="<<networkidx<<")"<<endl;

	// prepare AF_UNIX socket for the ipc with tasks
	struct sockaddr_un serveraddr2;
	ipcfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if(ipcfd < 0)
	{
		cout<<"[fileserver]AF_UNIX socket opening failed"<<endl;
		exit(-1);
	}

	// bind
	memset((void*) &serveraddr2, 0, sizeof(serveraddr2));
	serveraddr2.sun_family = AF_UNIX;
	strcpy(serveraddr2.sun_path, IPC_PATH);

	if(bind(ipcfd, (struct sockaddr *)&serveraddr2, SUN_LEN(&serveraddr2)) < 0)
	{
		cout<<"[fileserver]\033[0;31mIPC Binding failed\033[0m"<<endl;
		exit(-1);
	}

	// listen
	if(listen(ipcfd, BACKLOG) < 0)
	{
		cout<<"[master]Listening failed"<<endl;
		exit(-1);
	}

	// set the server fd and ipc fd as nonblocking mode
	fcntl(ipcfd, F_SETFL, O_NONBLOCK);
	fcntl(serverfd, F_SETFL, O_NONBLOCK);

	tmpfd = -1;

	// start loop which listen to connections and signals from clients and peers
	while(1)
	{
		// local clients
		tmpfd = accept(ipcfd, NULL, NULL);
		if(tmpfd > 0) // new file client is connected
		{
			// create new clients
			this->clients.push_back(new file_connclient(tmpfd));

			// set socket to be non-blocking socket to avoid deadlock
			fcntl(tmpfd, F_SETFL, O_NONBLOCK);
		}

		// remote client
		tmpfd = accept(serverfd, NULL, NULL);
		if(tmpfd > 0) // new file client is connected
		{
			// create new clients
			this->clients.push_back(new file_connclient(tmpfd));

			// set socket to be non-blocking socket to avoid deadlock
			fcntl(tmpfd, F_SETFL, O_NONBLOCK);
		}

		for(int i = 0; (unsigned)i < this->clients.size(); i++)
		{
			if(clients[i]->get_role() == UNDEFINED)
			{
				int readbytes = -1;

				readbytes = nbread(clients[i]->get_fd(), read_buf);
				if(readbytes > 0)
				{
					char* token;
					string filename;

					strcpy(write_buf, read_buf);

					token = strtok(read_buf, " "); // <- read or write

					// The message is either: Rread(raw), Iread(intermediate), Iwrite(intermediate), Owrite(output)
					if(strncmp(token, "Rread", 5) == 0)
					{
						// determine the candidate eclipse node which will have the data
						clients[i]->set_role(READ);
						string address;

						token = strtok(NULL, " "); // <- file name

						filename = token; // file name is name same as the dataname(key)

						// determine the location of data
						memset(read_buf, 0, BUF_SIZE);
						strcpy(read_buf, filename.c_str());

						uint32_t hashvalue = h(read_buf, HASHLENGTH);
						hashvalue = hashvalue%nodelist.size();

						address = nodelist[hashvalue];

						filebridge* thebridge = new filebridge(fbidclock++);
						bridges.push_back(thebridge);

						if(localhostname == address)
						{
							thebridge->set_srctype(DISK);
							thebridge->set_dsttype(CLIENT);
							thebridge->set_dstclient(clients[i]);
							thebridge->set_dataname(filename);
							thebridge->set_filename(filename);
							thebridge->set_dtype(RAW);

							// open read file and start sending data to client
							thebridge->open_readfile(filename);
						}
						else // distant
						{
							thebridge->set_srctype(PEER);
							thebridge->set_dsttype(CLIENT);
							thebridge->set_dstclient(clients[i]);
							thebridge->set_dataname(filename);
							thebridge->set_filename(filename);
							thebridge->set_dtype(RAW);

							// send message to the target peer
							string message;
							stringstream ss;
							ss << "Rread ";
							ss << filename;
							ss << " ";
							ss << thebridge->get_id();
							message = ss.str();

							memset(write_buf, 0, BUF_SIZE);
							strcpy(write_buf, message.c_str());

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<address<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

							filepeer* thepeer = find_peer(address);

							if(thepeer->msgbuf.size() > 1)
							{
								thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
								thepeer->msgbuf.push_back(new messagebuffer());
							}
							else
							{
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: "<<thepeer->get_address()<<endl;
								if(nbwritebuf(thepeer->get_fd(),
											write_buf, thepeer->msgbuf.back()) <= 0)
								{
									thepeer->msgbuf.push_back(new messagebuffer());
								}
							}

							//nbwrite(find_peer(address)->get_fd(), write_buf);
						}
					}
					else if(strncmp(token, "Iread", 5) == 0)
					{
						// determine the candidate eclipse node which will have the data
						string dataname;
						string address;
						char* token2;

						clients[i]->set_role(READ);

						token = strtok(NULL, " "); // <- file name
						filename = token;
						token2 = strtok(token, "_"); // <- .job
						token2 = strtok(NULL, "_"); // <- job index
						token2 = strtok(NULL, "_"); // <- data name
						dataname = token2;

						// determine the location of data by dataname(key)
						memset(read_buf, 0, BUF_SIZE);
						strcpy(read_buf, dataname.c_str());

						uint32_t hashvalue = h(read_buf, HASHLENGTH);
						hashvalue = hashvalue%nodelist.size();

						address = nodelist[hashvalue];

						filebridge* thebridge = new filebridge(fbidclock++);
						bridges.push_back(thebridge);

						if(localhostname == address)
						{
							thebridge->set_srctype(DISK);
							thebridge->set_dsttype(CLIENT);
							thebridge->set_dstclient(clients[i]);
							thebridge->set_dataname(dataname);
							thebridge->set_filename(filename);
							thebridge->set_dtype(INTERMEDIATE);

							// open read file and start sending data to client
							thebridge->open_readfile(filename);
						}
						else // distant
						{
							thebridge->set_srctype(PEER);
							thebridge->set_dsttype(CLIENT);
							thebridge->set_dstclient(clients[i]);
							thebridge->set_dataname(dataname);
							thebridge->set_filename(filename);
							thebridge->set_dtype(INTERMEDIATE);

							// send message to the target peer
							string message;
							stringstream ss;
							ss << "Iread ";
							ss << filename;
							ss << " ";
							ss << thebridge->get_id();
							message = ss.str();

							memset(write_buf, 0, BUF_SIZE);
							strcpy(write_buf, message.c_str());

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<address<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

							filepeer* thepeer = find_peer(address);

							if(thepeer->msgbuf.size() > 1)
							{
								thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
								thepeer->msgbuf.push_back(new messagebuffer());
							}
							else
							{
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: "<<thepeer->get_address()<<endl;
								if(nbwritebuf(thepeer->get_fd(),
											write_buf, thepeer->msgbuf.back()) <= 0)
								{
									thepeer->msgbuf.push_back(new messagebuffer());
								}
							}


							//nbwrite(find_peer(address)->get_fd(), write_buf);
						}
					}
					else if(strncmp(token, "Iwrite", 6) == 0)
					{
						// determine the candidate eclipse node which will have the data
						string dataname;
						string record;
						string address;
						char* token2;
						char* buf;
						clients[i]->set_role(WRITE);

						buf = read_buf;
						buf += strlen(token) + 1;

						token = strtok(NULL, " "); // <- file name
						buf += strlen(token) + 1; // keeps track of record position

						filename = token;
						token2 = strtok(token, "_"); // <- .job 
						token2 = strtok(NULL, "_"); // <- jobindex
						token2 = strtok(NULL, "_"); // <- data name
						dataname = token2;
						record = buf; // following data is record to be written

						// determine the location of data by dataname(key)
						memset(read_buf, 0, BUF_SIZE);
						strcpy(read_buf, dataname.c_str());

						uint32_t hashvalue = h(read_buf, HASHLENGTH);
						hashvalue = hashvalue%nodelist.size();

						address = nodelist[hashvalue];

						filebridge* thebridge = new filebridge(fbidclock++);
						// bridges.push_back(thebridge);

						if(localhostname == address)
						{
							// open write file to write data to disk
							thebridge->open_writefile(filename);
							thebridge->write_record(record, write_buf);

							delete thebridge;

							// clear the client
							if(clients[i]->msgbuf.size() > 1)
							{
								clients[i]->msgbuf.back()->set_endbuffer(clients[i]->get_fd());
								clients[i]->msgbuf.push_back(new messagebuffer());
							}
							else
							{
								close(clients[i]->get_fd());
								delete clients[i];
								clients.erase(clients.begin()+i);
								i--;
							}
						}
						else // distant
						{
							bridges.push_back(thebridge);

							// set up the bridge
							thebridge->set_srctype(PEER); // source of Ewrite
							thebridge->set_dsttype(CLIENT); // destination of Ewrite
							thebridge->set_dstclient(clients[i]);
							thebridge->set_dtype(INTERMEDIATE);

							// send message along with the record to the target peer
							string message;
							stringstream ss;
							ss << "Iwrite ";
							ss << filename;
							ss << " ";
							ss << thebridge->get_id();
							ss << " ";
							ss << record;
							message = ss.str();

							memset(write_buf, 0, BUF_SIZE);
							strcpy(write_buf, message.c_str());

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<address<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

							filepeer* thepeer = find_peer(address);
							if(thepeer->msgbuf.size() > 1)
							{
								thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
								thepeer->msgbuf.push_back(new messagebuffer());
							}
							else
							{
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: "<<thepeer->get_address()<<endl;
								if(nbwritebuf(thepeer->get_fd(),
											write_buf, thepeer->msgbuf.back()) <= 0)
								{
									thepeer->msgbuf.push_back(new messagebuffer());
								}
							}
						}
					}
					else if(strncmp(token, "Owrite", 6) == 0)
					{
						// determine the candidate eclipse node which will have the data
						string record;
						string address;
						char* buf;
						clients[i]->set_role(WRITE);

						buf = read_buf;
						buf += strlen(token) + 1;

						token = strtok(NULL, " "); // <- file name
						buf += strlen(token) + 1; // keeps track of record position

						filename = token;
						record = buf; // following data is record to be written

						// determine the location of data by dataname(key)
						memset(read_buf, 0, BUF_SIZE);
						strcpy(read_buf, filename.c_str());

						uint32_t hashvalue = h(read_buf, HASHLENGTH);
						hashvalue = hashvalue%nodelist.size();

						address = nodelist[hashvalue];

						filebridge* thebridge = new filebridge(fbidclock++);
						// bridges.push_back(thebridge);

						if(localhostname == address)
						{
							// open write file to write data to disk
							thebridge->open_writefile(filename);
							thebridge->write_record(record, write_buf);

							delete thebridge;

							// clear the client
							if(clients[i]->msgbuf.size() > 1)
							{
								clients[i]->msgbuf.back()->set_endbuffer(clients[i]->get_fd());
								clients[i]->msgbuf.push_back(new messagebuffer());
							}
							else
							{
								close(clients[i]->get_fd());
								delete clients[i];
								clients.erase(clients.begin()+i);
								i--;
							}
						}
						else // distant
						{
							bridges.push_back(thebridge);

							// set up the bridge
							thebridge->set_srctype(PEER); // source of Ewrite
							thebridge->set_dsttype(CLIENT); // destination of Ewrite
							thebridge->set_dstclient(clients[i]);
							thebridge->set_dtype(INTERMEDIATE);

							// send message along with the record to the target peer
							string message;
							stringstream ss;
							ss << "Owrite ";
							ss << filename;
							ss << " ";
							ss << thebridge->get_id();
							ss << " ";
							ss << record;
							message = ss.str();

							memset(write_buf, 0, BUF_SIZE);
							strcpy(write_buf, message.c_str());
							
//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<address<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

							filepeer* thepeer = find_peer(address);
							if(thepeer->msgbuf.size() > 1)
							{
								thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
								thepeer->msgbuf.push_back(new messagebuffer());
							}
							else
							{
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: "<<thepeer->get_address()<<endl;
								if(nbwritebuf(thepeer->get_fd(),
											write_buf, thepeer->msgbuf.back()) <= 0)
								{
									thepeer->msgbuf.push_back(new messagebuffer());
								}
							}

							//nbwrite(find_peer(address)->get_fd(), write_buf);
						}
					}
					else if(strncmp(token, "stop", 4) == 0)
					{
						// clear the process and exit
						for(int i = 0; (unsigned) i < clients.size(); i++)
							close(clients[i]->get_fd());

						for(int i = 0; (unsigned) i < peers.size(); i++)
							close(peers[i]->get_fd());

						close(ipcfd);
						close(serverfd);
						exit(0);
					}
					else
					{
						cout<<"[fileserver]Debugging: Unknown message";
					}
				}
			}
			else if(clients[i]->get_role() == READ) // READ role
			{
				// do nothing as default
			}
			else // WRITE role
			{
				// do nothing as default
			}
		}

		// receives read/write request or data stream
		for(int i = 0; (unsigned)i < peers.size(); i++)
		{
			// pass and continue when the conneciton to the peer have been closed
			if(peers[i]->get_fd() < 0)
				continue;

			int readbytes;
			readbytes = nbread(peers[i]->get_fd(), read_buf);

			if(readbytes > 0)
			{
				if(strncmp(read_buf, "Rread", 5) == 0)
				{
					string address;
					string filename;
					int dstid;
					char* token;

					token = strtok(read_buf, " "); // <- message type
					token = strtok(NULL, " "); // <- filename
					filename = token;
					token = strtok(NULL, " "); // <- dstid
					dstid = atoi(token);

					// if the target is cache
					{

					}

					// if the target is disk
					{
						filebridge* thebridge = new filebridge(fbidclock++);
						bridges.push_back(thebridge);

						thebridge->set_srctype(DISK);
						thebridge->set_dsttype(PEER);
						thebridge->set_dstid(dstid);
						thebridge->set_dstpeer(peers[i]);
						thebridge->set_dataname(filename);
						thebridge->set_filename(filename);
						thebridge->set_dtype(RAW);

						// open read file and start sending data to client
						thebridge->open_readfile(filename);
					}
				}
				else if(strncmp(read_buf, "Iread", 5) == 0)
				{
					string address;
					string filename;
					int dstid;
					char* token;

					token = strtok(read_buf, " "); // <- message type
					token = strtok(NULL, " "); // <- filename
					filename = token;
					cout<<"Iread file name: "<<filename<<endl;
					token = strtok(NULL, " "); // <- dstid
					dstid = atoi(token);

					// if the target is cache
					{

					}

					// if the target is disk
					{
						filebridge* thebridge = new filebridge(fbidclock++);
						bridges.push_back(thebridge);

						thebridge->set_srctype(DISK);
						thebridge->set_dsttype(PEER);
						thebridge->set_dstid(dstid);
						thebridge->set_dstpeer(peers[i]);
						thebridge->set_dataname(filename); // data name is wrong. but it doesn't matter at this time
						thebridge->set_filename(filename);
						thebridge->set_dtype(INTERMEDIATE);

						// open read file and start sending data to client
						thebridge->open_readfile(filename);
					}
				}
				else if(strncmp(read_buf, "Iwrite", 6) == 0)
				{
					string record;
					string address;
					string filename;
					char* token;
					char* buf;
					int bridgeid;

					buf = read_buf;

					token = strtok(read_buf, " "); // <- message type
					buf += strlen(token) + 1;
					token = strtok(NULL, " "); // <- filename
					buf += strlen(token) + 1;

					filename = token;

					token = strtok(NULL, " "); // <- bridge id
					buf += strlen(token) + 1;

					bridgeid = atoi(token);

					record = buf;

					filebridge* thebridge = new filebridge(fbidclock++);

					// if the target is cache
					{

					}

					// if the target is disk
					{
						thebridge->open_writefile(filename);
						thebridge->write_record(record, write_buf);

cout<<"record written: "<<record<<endl;

						stringstream ss;
						string message;
						ss << "Ewrite ";
						ss << bridgeid;
						message = ss.str();

						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, message.c_str());

						if(peers[i]->msgbuf.size() > 1)
						{
							peers[i]->msgbuf.back()->set_buffer(write_buf, peers[i]->get_fd());
							peers[i]->msgbuf.push_back(new messagebuffer());
						}
						else
						{
							if(nbwritebuf(peers[i]->get_fd(),
										write_buf, peers[i]->msgbuf.back()) <= 0)
							{
								peers[i]->msgbuf.push_back(new messagebuffer());
							}
						}

						delete thebridge;
					}
				}
				else if(strncmp(read_buf, "Owrite", 6) == 0)
				{
					string record;
					string address;
					string filename;
					char* token;
					char* buf;
					int bridgeid;

					buf = read_buf;

					token = strtok(read_buf, " "); // <- message type
					buf += strlen(token)+1;
					token = strtok(NULL, " "); // <- filename
					buf += strlen(token)+1;

					filename = token;

					token = strtok(NULL, " "); // <- bridge id
					buf += strlen(token)+1;

					bridgeid = atoi(token);

					record = buf;

					filebridge* thebridge = new filebridge(fbidclock++);

					// if the target is cache
					{

					}

					// if the target is disk
					{
						thebridge->open_writefile(filename);
						thebridge->write_record(record, write_buf);

cout<<"record written: "<<record<<endl;

						stringstream ss;
						string message;
						ss << "Ewrite ";
						ss << bridgeid;
						message = ss.str();

						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, message.c_str());

						if(peers[i]->msgbuf.size() > 1)
						{
							peers[i]->msgbuf.back()->set_buffer(write_buf, peers[i]->get_fd());
							peers[i]->msgbuf.push_back(new messagebuffer());
						}
						else
						{
							if(nbwritebuf(peers[i]->get_fd(),
									write_buf, peers[i]->msgbuf.back()) <= 0)
							{
								peers[i]->msgbuf.push_back(new messagebuffer());
							}
						}

						delete thebridge;
					}
				}
				else if(strncmp(read_buf, "Eread", 5) == 0) // end of file read stream notifiation
				{
					char* token;
					int id;
					int bridgeindex = -1;
					bridgetype dsttype;
					token = strtok(read_buf, " "); // <- Eread
					token = strtok(NULL, " "); // <- id of bridge

					id = atoi(token);

					for(int j = 0; (unsigned)j < bridges.size(); j++)
					{
						if(bridges[j]->get_id() == id)
						{
							bridgeindex = j;
							break;
						}
					}
if(bridgeindex == -1)
cout<<"bridge not found with that index"<<endl;

					dsttype = bridges[bridgeindex]->get_dsttype();

					if(dsttype == CLIENT)
					{
						file_connclient* theclient = bridges[bridgeindex]->get_dstclient();

						// clear the clients and bridges
						for(int j = 0; (unsigned)j < clients.size(); j++)
						{
							if(clients[j] == theclient)
							{
								if(theclient->msgbuf.size() > 1)
								{
									theclient->msgbuf.back()->set_endbuffer(theclient->get_fd());
									theclient->msgbuf.push_back(new messagebuffer());
								}
								else
								{
									close(clients[j]->get_fd());
									delete clients[j];
									clients.erase(clients.begin()+j);
								}
								break;
							}
						}

						delete bridges[bridgeindex];
						bridges.erase(bridges.begin()+bridgeindex);
					}
					else if(dsttype == PEER) // stroes the data into cache and send data to target node
					{
						// do nothing for now
					}
					cout<<"end of read"<<endl;
				}
				else if(strncmp(read_buf, "Ewrite", 6) == 0)
				{
					char* token;
					int bridgeid;
					int bridgeindex = -1;
					bridgetype dsttype;

					token = strtok(read_buf, " "); // <- Ewrite
					token = strtok(NULL, " "); // <- bridge id

					bridgeid = atoi(token);

					for(int j = 0; (unsigned)j < bridges.size(); j++)
					{
						if(bridges[j]->get_id() == bridgeid)
						{
							bridgeindex = j;
							break;
						}
					}
if(bridgeindex == -1)
cout<<"bridge not found with that index"<<endl;
					
					dsttype = bridges[bridgeindex]->get_dsttype();

					if(dsttype == CLIENT)
					{
						file_connclient* theclient = bridges[bridgeindex]->get_dstclient();

						// clear the clients and bridges
						
						for(int j = 0; (unsigned)j < clients.size(); j++)
						{
							if(clients[j] == theclient)
							{
								if(theclient->msgbuf.size() > 1)
								{
									theclient->msgbuf.back()->set_endbuffer(theclient->get_fd());
									theclient->msgbuf.push_back(new messagebuffer());
								}
								else
								{
									close(clients[j]->get_fd());
									delete clients[j];
									clients.erase(clients.begin()+j);
								}
								break;
							}
						}

						delete bridges[bridgeindex];
						bridges.erase(bridges.begin()+bridgeindex);
					}
					else if(dsttype == PEER)
					{
						// do nothing for now
					}
				}
				else // a filebridge id is passed. this is the case of data read stream
				{
					filebridge* thebridge;
					string record;
					char* token;
					char* buf;
					int id;
					bridgetype dsttype;

					buf = read_buf;
					token = strtok(read_buf, " "); // <- filebridge id

					id = atoi(token);
					buf += strlen(token) + 1; // <- record
					record = buf;

					for(int j = 0; (unsigned)j < bridges.size(); j++)
					{
						if(bridges[j]->get_id() == id)
						{
							thebridge = bridges[j];
						}
					}

					dsttype = thebridge->get_dsttype();

					if(dsttype == CLIENT)
					{
						file_connclient* theclient = thebridge->get_dstclient();
						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, record.c_str()); // send only the record

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to a client"<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

						if(theclient->msgbuf.size() > 1)
						{
							theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
							theclient->msgbuf.push_back(new messagebuffer());
						}
						else
						{
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: client"<<endl;
							if(nbwritebuf(theclient->get_fd(),
										write_buf, theclient->msgbuf.back()) <= 0)
							{
								theclient->msgbuf.push_back(new messagebuffer());
							}
						}

						//nbwrite(theclient->get_fd(), write_buf);
					}
					else if(dsttype == PEER) // stores the data into cache and send data to target node
					{
						// do nothing for now
					}
				}
			}
			else if(readbytes == 0)
			{
				cout<<"[fileserver]Debugging: Connection from a peer disconnected: "<<peers[i]->get_address()<<endl;

				// clear the peer
				close(peers[i]->get_fd());
				peers[i]->set_fd(-1);
			}
		}

		// process reading from the disk or cache and send the data to peer or client
		for(int i = 0; (unsigned)i < bridges.size(); i++)
		{
			if(bridges[i]->get_srctype() == CACHE)
			{
				// do nothing for now. there is no such a case currently
			}
			else if(bridges[i]->get_srctype() == DISK)
			{
				bool is_success;
				string record;
				is_success = bridges[i]->read_record(&record);
				if(is_success) // some remaining record
				{
					if(bridges[i]->get_dsttype() == CLIENT)
					{
						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, record.c_str());

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to a client"<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

						file_connclient* theclient = bridges[i]->get_dstclient();
						if(theclient->msgbuf.size() > 1)
						{
							theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
							theclient->msgbuf.push_back(new messagebuffer());
						}
						else
						{
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: client"<<endl;
							if(nbwritebuf(theclient->get_fd(),
										write_buf, theclient->msgbuf.back()) <= 0)
							{
								theclient->msgbuf.push_back(new messagebuffer());
							}
						}

						//nbwrite(bridges[i]->get_dstclient()->get_fd(), write_buf);
					}
					else if(bridges[i]->get_dsttype() == PEER)
					{
						stringstream ss;
						string message;
						ss << bridges[i]->get_dstid();
						message = ss.str();
						message.append(" ");
						message.append(record);

						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, message.c_str());

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

						filepeer* thepeer = bridges[i]->get_dstpeer();

						if(thepeer->msgbuf.size() > 1)
						{
							thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
							thepeer->msgbuf.push_back(new messagebuffer());
						}
						else
						{
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: "<<thepeer->get_address()<<endl;
							if(nbwritebuf(thepeer->get_fd(),
										write_buf, thepeer->msgbuf.back()) <= 0)
							{
								thepeer->msgbuf.push_back(new messagebuffer());
							}
						}

						//nbwrite(bridges[i]->get_dstpeer()->get_fd(), write_buf);
					}
				}
				else // end of data(file)
				{
					if(bridges[i]->get_dsttype() == CLIENT)
					{
						file_connclient* theclient = bridges[i]->get_dstclient();
						if(theclient->msgbuf.size() > 1)
						{
							theclient->msgbuf.back()->set_endbuffer(theclient->get_fd());
							theclient->msgbuf.push_back(new messagebuffer());
						}
						else
						{
							for(int j = 0; (unsigned)j < clients.size(); j++)
							{
								if(clients[j] == theclient)
								{
									close(theclient->get_fd());
									delete theclient;
									clients.erase(clients.begin()+j);
									break;
								}
							}
						}

						delete bridges[i];
						bridges.erase(bridges.begin()+i);
						i--;
					}
					else if(bridges[i]->get_dsttype() == PEER)
					{
						stringstream ss;
						string message;
						ss << "Eread ";
						ss << bridges[i]->get_dstid();
						message = ss.str();
						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, message.c_str());

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

						filepeer* thepeer = bridges[i]->get_dstpeer();

						if(thepeer->msgbuf.size() > 1)
						{
							thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
							thepeer->msgbuf.push_back(new messagebuffer());
						}
						else
						{
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: "<<thepeer->get_address()<<endl;
							if(nbwritebuf(thepeer->get_fd(),
										write_buf, thepeer->msgbuf.back()) <= 0)
							{
								thepeer->msgbuf.push_back(new messagebuffer());
							}
						}
						//nbwrite(bridges[i]->get_dstpeer()->get_fd(), write_buf);

						delete bridges[i];
						bridges.erase(bridges.begin()+i);
						i--;
					}
				}
			}
		}

		// process buffered stream through peers
		for(int i = 0; (unsigned)i < peers.size(); i++)
		{
			while(peers[i]->msgbuf.size() > 1)
			{
				memset(write_buf, 0, BUF_SIZE);
				strcpy(write_buf, peers[i]->msgbuf.front()->get_message().c_str());

//cout<<endl;
//cout<<"buffer flush try: "<<write_buf<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: "<<peers[i]->get_address()<<endl;

				if(nbwritebuf(peers[i]->get_fd(), write_buf,
					peers[i]->msgbuf.front()->get_remain(), peers[i]->msgbuf.front()) > 0) // successfully transmitted
				{
					delete peers[i]->msgbuf.front();
					peers[i]->msgbuf.erase(peers[i]->msgbuf.begin());
				}
				else // not transmitted completely
				{
					break;
				}
			}
		}

		// process buffered stream through clients
		for(int i = 0; (unsigned)i < clients.size(); i++)
		{
			while(clients[i]->msgbuf.size() > 1)
			{

				if(clients[i]->msgbuf.front()->is_end())
				{
					close(clients[i]->get_fd());
					delete clients[i];
					clients.erase(clients.begin()+i);
					i--;
					break;
				}
				else
				{
					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, clients[i]->msgbuf.front()->get_message().c_str());

					if(nbwritebuf(clients[i]->get_fd(), write_buf,
						clients[i]->msgbuf.front()->get_remain(), clients[i]->msgbuf.front()) > 0) // successfully transmitted
					{
						delete clients[i]->msgbuf.front();
						clients[i]->msgbuf.erase(clients[i]->msgbuf.begin());
					}
					else
					{
						break;
					}
				}
			}
		}
	}
	return 0;
}

filepeer* fileserver::find_peer(string address)
{
	for(int i = 0; (unsigned)i < peers.size(); i++)
	{
		if(peers[i]->get_address() == address)
			return peers[i];
	}

	cout<<"[fileserver]Debugging: no such a peer. in find_peer()"<<endl;
	return NULL;
}

filebridge* fileserver::find_bridge(int id)
{
	for(int i = 0; (unsigned)i < bridges.size(); i++)
	{
		if(bridges[i]->get_id() == id)
			return bridges[i];
	}

	cout<<"[fileserver]Debugging: no such a bridge. in find_bridge(), id: "<<id<<endl;
	return NULL;
}

#endif
