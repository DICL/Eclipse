#ifndef __FILESERVER__
#define __FILESERVER__

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <mapreduce/definitions.hh>
#include <common/hash.hh>
#include <common/msgaggregator.hh>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "file_connclient.hh"
#include <orthrus/histogram.hh>
#include <orthrus/cache.hh>
#include <orthrus/dataentry.hh>
#include "messagebuffer.hh"
#include "filepeer.hh"
#include "filebridge.hh"
#include "writecount.hh"
#include <sys/fcntl.h>

using namespace std;

class fileserver // each slave node has an object of fileserver
{
	private:
		int serverfd;
		int cacheserverfd;
		int ipcfd;
		int fbidclock;
		string localhostname;
		histogram* thehistogram;
		cache* thecache;
		vector<string> nodelist;
		vector<file_connclient*> clients;
		vector<filebridge*> bridges;
		//vector<file_connclient*> waitingclients;
		//vector<writecount*> writecounts;

		char read_buf[BUF_SIZE];
		char write_buf[BUF_SIZE];

	public:
		vector<filepeer*> peers;

		fileserver();
		filepeer* find_peer(string& address);
		filebridge* find_bridge(int id);
		//writecount* find_writecount(int id, int* idx);
		int run_server(int port, string master_address);
		bool write_file(string fname, string& record);
};

fileserver::fileserver()
{
	this->serverfd = -1;
	this->cacheserverfd = -1;
	this->ipcfd = -1;
	this->fbidclock = 0; // fb id starts from 0
}

int fileserver::run_server(int port, string master_address)
{
	int buffersize = 8388608; // 8 MB buffer size

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

	// connect to cacheserver
	{
		cacheserverfd = -1;
		struct sockaddr_in serveraddr;
		struct hostent *hp;

		// SOCK_STREAM -> tcp
		cacheserverfd = socket(AF_INET, SOCK_STREAM, 0);
		if(cacheserverfd < 0)
		{
			cout<<"[fileserver]Openning socket failed"<<endl;
			exit(1);
		}

		hp = gethostbyname(master_address.c_str());

		memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
		serveraddr.sin_family = AF_INET;
		memcpy(&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
		serveraddr.sin_port = htons(port);

		while(connect(cacheserverfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
		{
			cout<<"[fileserver]Cannot connect to the cache server. Retrying..."<<endl;

			//cout<<"\thost name: "<<nodelist[i]<<endl;
			usleep(100000);

			continue;
		}

		fcntl(cacheserverfd, F_SETFL, O_NONBLOCK);
		setsockopt(cacheserverfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
		setsockopt(cacheserverfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));
	}

	// connect to other peer eclipse nodes
	for(int i = 0; i < networkidx; i++)
	{
		int clientfd = -1;
		struct sockaddr_in serveraddr;
		struct hostent *hp = NULL;

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

		while(connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
		{
			cout<<"[fileserver]Cannot connect to: "<<nodelist[i]<<endl;
			cout<<"Retrying..."<<endl;

			usleep(100000);

			//cout<<"\thost name: "<<nodelist[i]<<endl;
			continue;
		}

		// register the file peer
		peers.push_back(new filepeer(clientfd, nodelist[i]));

		// set the peer fd as nonblocking mode
		fcntl(clientfd, F_SETFL, O_NONBLOCK);
		setsockopt(clientfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
		setsockopt(clientfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));
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

	// register the current node itself to the peer list
	peers.push_back(new filepeer(-1, localhostname));

	// register the other peers in order
	for(int i = networkidx + 1; (unsigned)i < nodelist.size(); i++)
	{
		peers.push_back(new filepeer(-1, nodelist[i]));
	}

	// listen connections from peers and complete the eclipse network
	for(int i = networkidx + 1; (unsigned)i < nodelist.size(); i++)
	{
		struct sockaddr_in connaddr;
		int addrlen = sizeof(connaddr);

		tmpfd = accept(fd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
		if(tmpfd > 0)
		{
			char* haddrp = inet_ntoa(connaddr.sin_addr);
			string address = haddrp;

			for(int j = networkidx + 1; (unsigned)j < nodelist.size(); j++)
			{
				if(peers[j]->get_address() == address)
				{
					peers[j]->set_fd(tmpfd);
				}
			}

			// set the peer fd as nonblocking mode
			fcntl(tmpfd, F_SETFL, O_NONBLOCK);
			setsockopt(tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
			setsockopt(tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));
		}
		else if(tmpfd < 0)// connection failed
		{
			// retry with same index
			i--;
			continue;
		}
		else
		{
			cout<<"connection closed......................."<<endl;
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
	setsockopt(ipcfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
	setsockopt(ipcfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));
	setsockopt(serverfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
	setsockopt(serverfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));

	tmpfd = -1;

	// initialize the histogram
	thehistogram = new histogram(nodelist.size());

	// initialize the local cache
	thecache = new cache(CACHESIZE);

	// prepare the message write buffer
	//for(int i = 0; (unsigned)i < peers.size(); i++)
	//{
		//if(i != networkidx)
		//{
			//peers[i]->writebuffer.configure_initial("write");
			//peers[i]->writebuffer.set_targetbuf(&(peers[i]->msgbuf));
		//}
	//}

struct timeval time_start;
struct timeval time_end;
//struct timeval time_start2;
//struct timeval time_end2;;
gettimeofday(&time_start, NULL);
gettimeofday(&time_end, NULL);
//unsigned timeslot1 = 0;
//unsigned timeslot2 = 0;
//unsigned timeslot3 = 0;
//unsigned timeslot4 = 0;
//unsigned timeslot5 = 0;
//unsigned timeslot6 = 0;
//unsigned timeslot7 = 0;
//unsigned timeslot8 = 0;
//unsigned milli1 = 0;
//unsigned milli2 = 0;
//unsigned milli3 = 0;
//unsigned milli4 = 0;
//unsigned milli5 = 0;
//unsigned milli6 = 0;
//unsigned milli7 = 0;
//unsigned milli8 = 0;


	// start main loop which listen to connections and signals from clients and peers
	while(1)
	{
//gettimeofday(&time_start2, NULL);

		// local clients
		tmpfd = accept(ipcfd, NULL, NULL);
		if(tmpfd > 0) // new file client is connected
		{
			// create new clients
			this->clients.push_back(new file_connclient(tmpfd));

			// set socket to be non-blocking socket to avoid deadlock
			fcntl(tmpfd, F_SETFL, O_NONBLOCK);
			setsockopt(tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
			setsockopt(tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));
		}

		// remote client
		tmpfd = accept(serverfd, NULL, NULL);
		if(tmpfd > 0) // new file client is connected
		{
			// create new clients
			this->clients.push_back(new file_connclient(tmpfd));

			// set socket to be non-blocking socket to avoid deadlock
			fcntl(tmpfd, F_SETFL, O_NONBLOCK);
			setsockopt(tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t)sizeof(buffersize));
			setsockopt(tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t)sizeof(buffersize));
		}

//gettimeofday(&time_end2, NULL);
//timeslot1 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);

		for(int i = 0; (unsigned)i < this->clients.size(); i++)
		{
			int readbytes = -1;

			readbytes = nbread(clients[i]->get_fd(), this->read_buf);
			if(readbytes > 0)
			{
				char* token;
				string filename;

				memset(write_buf, 0, BUF_SIZE);
				strcpy(write_buf, this->read_buf);

				token = strtok(this->read_buf, " \n"); // <- read or write

				// The message is either: Rread(raw), Iread(intermediate), Iwrite(intermediate), Owrite(output)
				if(strncmp(token, "Rread", 5) == 0)
				{
					// determine the candidate eclipse node which will have the data
					string address;

					//clients[i]->set_role(READ);

					token = strtok(NULL, " "); // <- file name

					filename = token; // file name is same as the dataname(key)

					// determine the cache location of data
					memset(this->read_buf, 0, BUF_SIZE);
					strcpy(this->read_buf, filename.c_str());

					int index;
					uint32_t hashvalue = h(this->read_buf, HASHLENGTH);
					index = thehistogram->get_index(hashvalue);

					address = nodelist[index];

					filebridge* thebridge = new filebridge(fbidclock++);
					bridges.push_back(thebridge);

					if(address == localhostname) // local cache data
					{
						dataentry* theentry = thecache->lookup(filename);

						if(theentry == NULL) // when data is not in cache
						{
							cout<<"\033[0;31mCache miss\033[0m"<<endl;

							// set a entry writer
							dataentry* newentry = new dataentry(filename, hashvalue);
							thecache->new_entry(newentry);
							entrywriter* thewriter = new entrywriter(newentry);

							// determine the DHT file location
							hashvalue = hashvalue%nodelist.size();

							address = nodelist[hashvalue];

							if(localhostname == address)
							{
								// 1. read data from disk 
								// 2. store it in the cache 
								// 3. send it to client

								thebridge->set_srctype(DISK);
								thebridge->set_dsttype(CLIENT);
								thebridge->set_entrywriter(thewriter);
								thebridge->set_dstclient(clients[i]);

								thebridge->writebuffer = new msgaggregator(clients[i]->get_fd());
								thebridge->writebuffer->configure_initial("");
								thebridge->writebuffer->set_msgbuf(&clients[i]->msgbuf);
								thebridge->writebuffer->set_dwriter(thewriter);
								//thebridge->set_filename(filename);
								//thebridge->set_dataname(filename);
								//thebridge->set_dtype(RAW);

								// open read file and start sending data to client
								thebridge->open_readfile(filename);
							}
							else // remote DHT peer
							{
								// 1. request to the DHT peer (read from peer)
								// 2. store it in the cache
								// 3. send it to client

								thebridge->set_srctype(PEER);
								thebridge->set_dsttype(CLIENT);
								thebridge->set_entrywriter(thewriter);
								thebridge->set_dstclient(clients[i]);

								// thebridge->set_filename(filename);
								// thebridge->set_dataname(filename);
								// thebridge->set_dtype(RAW);

								// send message to the target peer
								string message;
								stringstream ss;
								ss << "RDread ";
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
									if(nbwritebuf(thepeer->get_fd(),
												write_buf, thepeer->msgbuf.back()) <= 0)
									{
										thepeer->msgbuf.push_back(new messagebuffer());
									}
								}
							}
						}
						else // when the data is in the cache
						{
							// 1. read from cache
							// 2. and send it to client

							// set a entry reader
							entryreader* thereader = new entryreader(theentry);

							cout<<"\033[0;32mCache hit\033[0m"<<endl;

							thebridge->set_srctype(CACHE);
							thebridge->set_dsttype(CLIENT);
							thebridge->set_entryreader(thereader);
							thebridge->set_dstclient(clients[i]);
							//thebridge->set_filename(filename);
							//thebridge->set_dataname(filename);
							//thebridge->set_dtype(RAW);
						}
					}
					else // remote cache peer
					{
						// 1. request to the remote cache peer
						// 2. send it to clienet

						thebridge->set_srctype(PEER);
						thebridge->set_dsttype(CLIENT);
						thebridge->set_dstclient(clients[i]);
						//thebridge->set_filename(filename);
						//thebridge->set_dataname(filename);
						//thebridge->set_dtype(RAW);

						// send message to the target peer
						string message;
						stringstream ss;
						ss << "RCread ";
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
							if(nbwritebuf(thepeer->get_fd(),
										write_buf, thepeer->msgbuf.back()) <= 0)
							{
								thepeer->msgbuf.push_back(new messagebuffer());
							}
						}
					}
				}
				else if(strncmp(token, "Iread", 5) == 0)
				{
//cout<<"\t\tIread request from client"<<endl;
					// determine the candidate eclipse node which will have the data
					string address;

					//clients[i]->set_role(READ);

					token = strtok(NULL, " "); // <- file name

					filename = token;

					// determine the location of data by dataname(key)
					memset(this->read_buf, 0, BUF_SIZE);
					strcpy(this->read_buf, filename.c_str());

					uint32_t hashvalue = h(this->read_buf, HASHLENGTH);
					hashvalue = hashvalue%nodelist.size();

					address = nodelist[hashvalue];

					filebridge* thebridge = new filebridge(fbidclock++);
					bridges.push_back(thebridge);

					if(localhostname == address)
					{
						thebridge->set_srctype(DISK);
						thebridge->set_dsttype(CLIENT);
						thebridge->set_dstclient(clients[i]);
						thebridge->writebuffer = new msgaggregator(clients[i]->get_fd());
						thebridge->writebuffer->configure_initial("");
						thebridge->writebuffer->set_msgbuf(&clients[i]->msgbuf);

						//thebridge->set_filename(filename);
						//thebridge->set_dataname(filename);
						//thebridge->set_dtype(INTERMEDIATE);

						// open read file and start sending data to client
						thebridge->open_readfile(filename);
					}
					else // distant
					{
						thebridge->set_srctype(PEER);
						thebridge->set_dsttype(CLIENT);
						thebridge->set_dstclient(clients[i]);
						//thebridge->set_filename(filename);
						//thebridge->set_dataname(filename);
						//thebridge->set_dtype(INTERMEDIATE);

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
				else if(strncmp(token, "write", 5) == 0)
				{
					// determine the candidate eclipse node which will have the data
					string record;
					string address;
					//clients[i]->set_role(WRITE);

					token = strtok(NULL, " "); // tokenize first filename

					while(token != NULL)
					{
						filename = token; // filename
						token = strtok(NULL, "\n"); // tokenize record of file
						record = token;
						token = strtok(NULL, " "); // tokenize next file name

						// determine the location of data by filename
						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, filename.c_str());

						uint32_t hashvalue = h(write_buf, HASHLENGTH);
						hashvalue = hashvalue%nodelist.size();

						address = nodelist[hashvalue];

						if(localhostname == address)
						{
							write_file(filename, record);
						}
						else
						{
							string message = filename;
							message.append(" ");
							message.append(record);

							filepeer* thepeer = peers[hashvalue];

							if(clients[i]->thecount == NULL)
								cout<<"[fileserver]The write id is not set before the write request"<<endl;

							clients[i]->thecount->add_peer(hashvalue);

							thepeer->writebuffer.add_record(message);
						}
					}

					/*
					if(localhostname == address)
					{
						// open write file to write data to disk
						write_file(filename, record);
						//filebridge* thebridge = new filebridge(fbidclock++);
						//thebridge->open_writefile(filename);
						//thebridge->write_record(record, write_buf);

						//delete thebridge;
					}
					else // distant
					{
						// bridges.push_back(thebridge);

						// set up the bridge
						// thebridge->set_srctype(PEER); // source of Ewrite
						// thebridge->set_dsttype(CLIENT); // destination of Ewrite
						// thebridge->set_dstclient(NULL);
						// thebridge->set_dtype(INTERMEDIATE);
						// thebridge->set_writeid(writeid);

						// send message along with the record to the target peer
						string message;
						stringstream ss;
						ss << "write ";
						ss << filename;
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

						filepeer* thepeer = peers[hashvalue];

						if(clients[i]->thecount == NULL)
							cout<<"[fileserver]The write id is not set before the write request"<<endl;

						clients[i]->thecount->add_peer(hashvalue);

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
					*/
				}
				else if(strncmp(token, "Wwrite", 6) == 0)
				{
					writecount* thecount = clients[i]->thecount;

					if(thecount == NULL)
					{
						cout<<"[fileserver]The write count of a client is not set before Wwrite message"<<endl;

						// close the client
						close(clients[i]->get_fd());
						delete clients[i];
						clients.erase(clients.begin()+i);
						i--;
					}
					else if(thecount->peerids.size() == 0)
					{
						// clear the count
						delete thecount;
						clients[i]->thecount = NULL;

						// close the client
						close(clients[i]->get_fd());
						delete clients[i];
						clients.erase(clients.begin()+i);
						i--;
					}
					else
					{
						for(set<int>::iterator it = thecount->peerids.begin(); it != thecount->peerids.end(); it++)
						{
							filepeer* thepeer = peers[*it];

							// send message to the target peer
							string message;
							stringstream ss;
							ss << "Wack ";
							ss << thecount->get_id();
							message = ss.str();

							memset(write_buf, 0, BUF_SIZE);
							strcpy(write_buf, message.c_str());

							// flush the buffer to guarantee the order consistency
							thepeer->writebuffer.flush();

							if(thepeer->msgbuf.size() > 1)
							{
								thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
								thepeer->msgbuf.push_back(new messagebuffer());
							}
							else
							{
								if(nbwritebuf(thepeer->get_fd(), 
											write_buf, thepeer->msgbuf.back()) <= 0)
								{
									thepeer->msgbuf.push_back(new messagebuffer());
								}
							}
						}
					}
				}
				else if(strncmp(token, "Wid", 6) == 0)
				{
					int writeid;

					token = strtok(NULL, " "); // <- write id
					writeid = atoi(token);

					if(clients[i]->thecount != NULL)
						cout<<"[fileserver]The write count of the client already set"<<endl;

					// create writecount with writeid
					clients[i]->thecount = new writecount(writeid);
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

				// enable loop acceleration
				//i--;
				//continue;
			}
		}
//gettimeofday(&time_end2, NULL);
//timeslot2 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);

		// receives read/write request or data stream
		for(int i = 0; (unsigned)i < peers.size(); i++)
		{
			// pass and continue when the conneciton to the peer have been closed
			if(peers[i]->get_fd() < 0)
				continue;

			int readbytes;
			readbytes = nbread(peers[i]->get_fd(), this->read_buf);

			if(readbytes > 0)
			{
				if(strncmp(this->read_buf, "RCread", 6) == 0) // read request to cache
				{
					string address;
					string filename;
					int dstid;
					string sdstid;
					char* token;

					token = strtok(this->read_buf, " "); // <- message type
					token = strtok(NULL, " "); // <- filename
					filename = token;
					token = strtok(NULL, " "); // <- dstid
					sdstid = token;
					dstid = atoi(token);

					dataentry* theentry = thecache->lookup(filename);

					filebridge* thebridge = new filebridge(fbidclock++);
					bridges.push_back(thebridge);

					if(theentry == NULL) // when data is not in cache
					{
						cout<<"\033[0;31mCache miss\033[0m"<<endl;

						// determine hash value from file name
						memset(this->read_buf, 0, BUF_SIZE);
						strcpy(this->read_buf, filename.c_str());
						unsigned hashvalue = h(this->read_buf, HASHLENGTH);

						// set a entry writer
						dataentry* newentry = new dataentry(filename, hashvalue);
						thecache->new_entry(newentry);
						entrywriter* thewriter = new entrywriter(newentry);

						// determine the DHT file location
						hashvalue = hashvalue%nodelist.size();

						address = nodelist[hashvalue];

						if(localhostname == address) // local DHT data
						{
							// 1. read data from disk
							// 2. store it in the cache
							// 3. send it to peer

							thebridge->set_srctype(DISK);
							thebridge->set_dsttype(PEER);
							thebridge->set_entrywriter(thewriter);
							thebridge->set_dstpeer(peers[i]);
							thebridge->set_dstid(dstid);

							thebridge->writebuffer = new msgaggregator(peers[i]->get_fd());
							sdstid.append("\n");
							thebridge->writebuffer->configure_initial(sdstid);
							thebridge->writebuffer->set_msgbuf(&peers[i]->msgbuf);
							thebridge->writebuffer->set_dwriter(thewriter);
							//thebridge->set_filename(filename);
							//thebridge->set_dataname(filename);
							//thebridge->set_dtype(RAW);

							// open read file and start sending data to peer
							thebridge->open_readfile(filename);
						}
						else // remote DHT peer
						{
							// 1. request to the DHT peer (read from peer)
							// 2. store it in the cache
							// 3. eend it to client

							thebridge->set_srctype(PEER);
							thebridge->set_dsttype(PEER);
							thebridge->set_entrywriter(thewriter);
							thebridge->set_dstpeer(peers[i]);
							thebridge->set_dstid(dstid);
							//thebridge->set_filename(filename);
							//thebridge->set_dataname(filename);
							//thebridge->set_dtype(RAW);

							// send message to the target peer
							string message;
							stringstream ss;
							ss << "RDread ";
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

							filepeer* thepeer = peers[hashvalue];

							if(thepeer->msgbuf.size() > 1)
							{
								thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
								thepeer->msgbuf.push_back(new messagebuffer());
								//continue; // escape the acceleration loop
							}
							else
							{
								if(nbwritebuf(thepeer->get_fd(),
											write_buf, thepeer->msgbuf.back()) <= 0)
								{
									thepeer->msgbuf.push_back(new messagebuffer());
									//continue; // escape the acceleration loop
								}
							}
						}
					}
					else // when the data is in the cache
					{
						// 1. read from cache
						// 2. and send it to peer

						cout<<"\033[0;32mCache hit\033[0m"<<endl;

						// set a entry reader
						entryreader* thereader = new entryreader(theentry);

						thebridge->set_srctype(CACHE);
						thebridge->set_dsttype(PEER);
						thebridge->set_entryreader(thereader);
						thebridge->set_dstpeer(peers[i]);
						thebridge->set_dstid(dstid);
						//thebridge->set_filename(filename);
						//thebridge->set_dataname(filename);
						//thebridge->set_dtype(RAW);
					}
				}
				else if(strncmp(this->read_buf, "RDread", 6) == 0) // read request to disk
				{
					string address;
					string filename;
					int dstid;
					string sdstid;
					char* token;

					token = strtok(this->read_buf, " "); // <- message type
					token = strtok(NULL, " "); // <- filename
					filename = token;
					token = strtok(NULL, " "); // <- dstid
					sdstid = token;
					dstid = atoi(token);

					filebridge* thebridge = new filebridge(fbidclock++);
					bridges.push_back(thebridge);

					thebridge->set_srctype(DISK);
					thebridge->set_dsttype(PEER);
					thebridge->set_dstid(dstid);
					thebridge->set_dstpeer(peers[i]);

					thebridge->writebuffer = new msgaggregator(peers[i]->get_fd());
					sdstid.append("\n");
					thebridge->writebuffer->configure_initial(sdstid);
					thebridge->writebuffer->set_msgbuf(&peers[i]->msgbuf);

					//thebridge->set_filename(filename);
					//thebridge->set_dataname(filename);
					//thebridge->set_dtype(RAW);

					// open read file and start sending data to client
					thebridge->open_readfile(filename);
				}
				else if(strncmp(this->read_buf, "Iread", 5) == 0)
				{
					string address;
					string filename;
					int dstid;
					string sdstid;
					char* token;

					token = strtok(this->read_buf, " "); // <- message type
					token = strtok(NULL, " "); // <- filename
					filename = token;
//cout<<"Iread file name: "<<filename<<endl;
					token = strtok(NULL, " "); // <- dstid
					sdstid = token;
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

						thebridge->writebuffer = new msgaggregator(peers[i]->get_fd());
						sdstid.append("\n");
						thebridge->writebuffer->configure_initial(sdstid);
						thebridge->writebuffer->set_msgbuf(&peers[i]->msgbuf);
						//thebridge->set_filename(filename);
						//thebridge->set_dataname(filename); // data name is wrong. but it doesn't matter at this time
						//thebridge->set_dtype(INTERMEDIATE);

						// open read file and start sending data to client
						thebridge->open_readfile(filename);
					}
				}
				else if(strncmp(this->read_buf, "write", 5) == 0)
				{
					string record;
					string address;
					string filename;
					char* token;

					token = strtok(this->read_buf, "\n"); // <- "write"

					token = strtok(NULL, " "); // tokenize first filename
					
					while(token != NULL)
					{
						filename = token; // filename
						token = strtok(NULL, "\n"); // tokenize record of file
						record = token;
						token = strtok(NULL, " "); // tokenize next file name

						// !!!in current implementation, all write messages from peer are all to local disk
						// if the target is cache
						{

						}

						// if the target is disk
						write_file(filename, record);
					}

					/*
					// if the target is cache
					{

					}

					// if the target is disk
					{
						write_file(filename, record);
						//filebridge* thebridge = new filebridge(fbidclock++);
						//thebridge->open_writefile(filename);
						//thebridge->write_record(record, write_buf);

//cout<<"record written: "<<record<<endl;

						// clear the bridge
						//delete thebridge;
					}
					*/
				}
				else if(strncmp(this->read_buf, "Eread", 5) == 0) // end of file read stream notification
				{
					char* token;
					int id;
					int bridgeindex = -1;
					bridgetype dsttype;
					token = strtok(this->read_buf, " "); // <- Eread
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

					// finish write to cache if writing was ongoing
					entrywriter* thewriter = bridges[bridgeindex]->get_entrywriter();
					if(thewriter != NULL)
					{
						thewriter->complete();
						delete thewriter;
						thewriter = NULL;
					}

					if(dsttype == CLIENT)
					{
//cout<<"\033[0;32mEread received\033[0m"<<endl;

						file_connclient* theclient = bridges[bridgeindex]->get_dstclient();

/*
						// clear the client and bridge
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
*/

						// send NULL packet to the client
						memset(write_buf, 0, BUF_CUT);
						write_buf[0] = -1;

						if(theclient->msgbuf.size() > 1)
						{
//cout<<"\tgoes to buffer"<<endl;
							theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
							theclient->msgbuf.push_back(new messagebuffer());
						}
						else
						{

							if(nbwritebuf(theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
							{
//cout<<"\tgoes to buffer"<<endl;
								theclient->msgbuf.push_back(new messagebuffer());
							}
//else
//cout<<"\tsent directly"<<endl;

						}

						// clear the bridge
						delete bridges[bridgeindex];
						bridges.erase(bridges.begin()+bridgeindex);
					}
					else if(dsttype == PEER) // stores the data into cache and send data to target node
					{
						string message;
						stringstream ss;
						ss << "Eread ";
						ss << bridges[bridgeindex]->get_dstid();
						message = ss.str();
						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, message.c_str());

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;

						filepeer* thepeer = bridges[bridgeindex]->get_dstpeer();

						if(thepeer->msgbuf.size() > 1)
						{
							thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
							thepeer->msgbuf.push_back(new messagebuffer());
						}
						else
						{
							if(nbwritebuf(thepeer->get_fd(),
									write_buf, thepeer->msgbuf.back()) <= 0)
							{
								thepeer->msgbuf.push_back(new messagebuffer());
							}
						}

						// clear the bridge
						delete bridges[bridgeindex];
						bridges.erase(bridges.begin()+bridgeindex);
					}
//cout<<"end of read"<<endl;
				}
				else if(strncmp(this->read_buf, "Wack", 4) == 0)
				{
//cout<<"\t\tWack"<<endl;
					string message;
					stringstream ss;
					int writeid;
					char* token;

					token = strtok(this->read_buf, " "); // <- Wack
					token = strtok(NULL, " "); // <- write id

					writeid = atoi(token);

					ss << "Wre ";
					ss << writeid;
					message = ss.str();

					// write back immediately
					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, message.c_str());

					if(peers[i]->msgbuf.size() > 1)
					{
						peers[i]->msgbuf.back()->set_buffer(write_buf, peers[i]->get_fd());
						peers[i]->msgbuf.push_back(new messagebuffer());
						//continue; // escape acceleration loop
					}
					else
					{
						if(nbwritebuf(peers[i]->get_fd(), write_buf, peers[i]->msgbuf.back()) <= 0)
						{
							peers[i]->msgbuf.push_back(new messagebuffer());
							//continue; // escape acceleration loop
						}
					}
				}
				else if(strncmp(this->read_buf, "Wre", 3) == 0)
				{
//cout<<"\t\t\t\tWre"<<endl;
					char* token;
					int writeid;
					int clientidx = -1;

					token = strtok(this->read_buf, " "); // <- Wre
					token = strtok(NULL, " "); // <- write id
					writeid = atoi(token);

					writecount* thecount = NULL;

					for(int j = 0; (unsigned)j < clients.size(); j++)
					{
						if(clients[j]->thecount != NULL)
						{
							if(clients[j]->thecount->get_id() == writeid)
							{
								clientidx = j;
								thecount = clients[j]->thecount;
							}
						}
					}
					
					if(thecount == NULL)
					{
						cout<<"[fileserver]Unexpected NULL pointer..."<<endl;
					}

					thecount->clear_peer(i);

					if(thecount->peerids.size() == 0)
					{
/*
						// call the target client and close it, clear it
						for(int j = 0; (unsigned)j < waitingclients.size(); j++)
						{
							if(waitingclients[j]->get_writeid() == writeid)
							{
								// close the client
								close(waitingclients[j]->get_fd());

								// clear the client
								delete waitingclients[j];
								waitingclients.erase(waitingclients.begin()+j);
							}
						}
*/
						// close the target client and clear it
						close(clients[clientidx]->get_fd());

						// clear the client
						delete clients[clientidx];
						clients.erase(clients.begin() + clientidx);

/*
						// clear the count
						delete writecounts[countidx];
						writecounts.erase(writecounts.begin()+countidx);
*/
					}
				}
/*
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
						// clear the write count
						writecount* thecount = find_writecount(bridges[bridgeindex]->get_writeid());
						if(thecount == NULL)
						{
							cout<<"[fileserver]Unexpected NULL pointer from find_writecount()."<<endl;
							exit(1);
						}
						thecount->decrement();

						// clear the and bridge
						delete bridges[bridgeindex];
						bridges.erase(bridges.begin()+bridgeindex);
					}
					else if(dsttype == PEER)
					{
						// do nothing for now
					}
				}
*/
				else // a filebridge id is passed. this is the case of data read stream
				{
					filebridge* thebridge;
					string record;
					char* token;
					char* buf;
					int id;
					bridgetype dsttype;

					buf = this->read_buf;
					token = strtok(this->read_buf, "\n"); // <- filebridge id


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
						// write to cache if writing was ongoing
						entrywriter* thewriter = thebridge->get_entrywriter();
						if(thewriter != NULL)
						{
							thewriter->write_record(record);
						}

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
							//continue; // escape acceleration loop
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
								//continue; // escape acceleration loop
							}
						}

						//nbwrite(theclient->get_fd(), write_buf);
					}
					else if(dsttype == PEER) // stores the data into cache and send data to target node
					{
						// write to cache if writing was ongoing
						entrywriter* thewriter = thebridge->get_entrywriter();
						if(thewriter != NULL)
						{
							thewriter->write_record(record);
						}

						string message;
						stringstream ss;
						ss << thebridge->get_dstid();
						message = ss.str();
						message.append("\n");
						message.append(record);

						memset(write_buf, 0, BUF_SIZE);
						strcpy(write_buf, message.c_str());

//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;
						filepeer* thepeer = thebridge->get_dstpeer();

						if(thepeer->msgbuf.size() > 1)
						{
							thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
							thepeer->msgbuf.push_back(new messagebuffer());
							//continue; // escape acceleration loop
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
								//continue; // escape acceleration loop
							}
						}
					}
				}

				// enable loop acceleration
				//i--;
				//continue;
			}
			else if(readbytes == 0)
			{
				cout<<"[fileserver]Debugging: Connection from a peer disconnected: "<<peers[i]->get_address()<<endl;

				// clear the peer
				close(peers[i]->get_fd());
				peers[i]->set_fd(-1);
			}
		}
//gettimeofday(&time_end2, NULL);
//timeslot3 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);

		// process reading from the disk or cache and send the data to peer or client
		for(int i = 0; (unsigned)i < bridges.size(); i++)
		{
			//for(int accel = 0; accel < 1000; accel++) // accelerate the reading speed by 1000
			//{
				if(bridges[i]->get_srctype() == CACHE)
				{
					bool ret;
					string record;
					memset(write_buf, 0, BUF_SIZE);

					if(bridges[i]->get_dsttype() == CLIENT)
					{
						ret = bridges[i]->get_entryreader()->read_record(record);

						strcpy(write_buf, record.c_str());
					}
					else if(bridges[i]->get_dsttype() == PEER)
					{
						stringstream ss;
						string message;
						ss << bridges[i]->get_dstid();
						message = ss.str();
						message.append("\n");

						ret = bridges[i]->get_entryreader()->read_record(record);

						message.append(record);

						strcpy(write_buf, message.c_str());
					}

					if(ret) // successfully read
					{
						if(bridges[i]->get_dsttype() == CLIENT)
						{
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
						}
						else if(bridges[i]->get_dsttype() == PEER)
						{
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
								if(nbwritebuf(thepeer->get_fd(), write_buf, thepeer->msgbuf.back()) <= 0)
								{
									thepeer->msgbuf.push_back(new messagebuffer());
								}
							}
						}
					}
					else // no more record
					{
//cout<<"\033[0;33mEread sent!\033[0m"<<endl;
						delete bridges[i]->get_entryreader();

						if(bridges[i]->get_dsttype() == CLIENT)
						{
//cout<<"\033[0;32mEread received\033[0m"<<endl;
							file_connclient* theclient = bridges[i]->get_dstclient();

							// send NULL packet to the client
							memset(write_buf, 0, BUF_CUT);
							write_buf[0] = -1;

							if(theclient->msgbuf.size() > 1)
							{
//cout<<"\tgoes to buffer"<<endl;
								theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
								theclient->msgbuf.push_back(new messagebuffer());
							}
							else
							{
								if(nbwritebuf(theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
								{
//cout<<"\tgoes to buffer"<<endl;
									theclient->msgbuf.push_back(new messagebuffer());
								}
//else
//cout<<"\tsent directly"<<endl;
							}
						}
						else if(bridges[i]->get_dsttype() == PEER)
						{
							stringstream ss1;
							string message1;
							ss1 << "Eread ";
							ss1 << bridges[i]->get_dstid();
							message1 = ss1.str();
							memset(write_buf, 0, BUF_SIZE);
							strcpy(write_buf, message1.c_str());

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
						}

						delete bridges[i];
						bridges.erase(bridges.begin()+i);
						i--;
						// break the accel loop
						//break;
					}
				}
				else if(bridges[i]->get_srctype() == DISK)
				{
					bool is_success;
					string record;
					is_success = bridges[i]->read_record(record);

					while(is_success) // some remaining record
					{
						if(bridges[i]->writebuffer->add_record(record))
							break;

						is_success = bridges[i]->read_record(record);
					}

					if(!is_success) // no more record to read
					{
//cout<<"\033[0;33mEread sent!\033[0m"<<endl;
						// flush the write buffer
						bridges[i]->writebuffer->flush();

						// complete writing to cache if writing was ongoing
						entrywriter* thewriter = bridges[i]->get_entrywriter();

						if(thewriter != NULL)
						{
							thewriter->complete();
							delete thewriter;
						}

						if(bridges[i]->get_dsttype() == CLIENT)
						{
//cout<<"\033[0;32mEread received\033[0m"<<endl;
							file_connclient* theclient = bridges[i]->get_dstclient();

							// send NULL packet to the client
							memset(write_buf, 0, BUF_CUT);
							write_buf[0] = -1;

							if(theclient->msgbuf.size() > 1)
							{
//cout<<"\tgoes to buffer"<<endl;
								theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
								theclient->msgbuf.push_back(new messagebuffer());
							}
							else
							{
								if(nbwritebuf(theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
								{
//cout<<"\tgoes to buffer"<<endl;
									theclient->msgbuf.push_back(new messagebuffer());
								}
//else
//cout<<"\tsent directly"<<endl;
							}

							// clear the bridge
							delete bridges[i];
							bridges.erase(bridges.begin()+i);
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

							filepeer* thepeer = bridges[i]->get_dstpeer();

							if(thepeer->msgbuf.size() > 1)
							{
								thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
								thepeer->msgbuf.push_back(new messagebuffer());
							}
							else
							{
								if(nbwritebuf(thepeer->get_fd(),
											write_buf, thepeer->msgbuf.back()) <= 0)
								{
									thepeer->msgbuf.push_back(new messagebuffer());
								}
							}

							delete bridges[i];
							bridges.erase(bridges.begin()+i);
						}
					}

/*
					if(is_success) // some remaining record
					{
						// write to cache if writing was ongoing
						entrywriter* thewriter = bridges[i]->get_entrywriter();
						if(thewriter != NULL)
						{
							thewriter->write_record(record);
						}

						if(bridges[i]->get_dsttype() == CLIENT)
						{
							memset(write_buf, 0, BUF_SIZE);
							strcpy(write_buf, record.c_str());

							file_connclient* theclient = bridges[i]->get_dstclient();
							if(theclient->msgbuf.size() > 1)
							{
								theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
								theclient->msgbuf.push_back(new messagebuffer());
							}
							else
							{
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

							filepeer* thepeer = bridges[i]->get_dstpeer();

							if(thepeer->msgbuf.size() > 1)
							{
								thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
								thepeer->msgbuf.push_back(new messagebuffer());
							}
							else
							{
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
						// write to cache if writing was ongoing
						entrywriter* thewriter = bridges[i]->get_entrywriter();

						if(thewriter != NULL)
						{
							thewriter->complete();
							delete thewriter;
							thewriter = NULL;
						}

						if(bridges[i]->get_dsttype() == CLIENT)
						{
							file_connclient* theclient = bridges[i]->get_dstclient();

							// send NULL packet to the client
							memset(write_buf, -1, BUF_CUT);

							if(theclient->msgbuf.size() > 1)
							{
								theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
								theclient->msgbuf.push_back(new messagebuffer());
							}
							else
							{
								if(nbwritebuf(theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
								{
									theclient->msgbuf.push_back(new messagebuffer());
								}
							}

							// clear the bridge
							delete bridges[i];
							bridges.erase(bridges.begin()+i);
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
						}

						// break the accel loop
						//break;
					}
*/
				}
			//}
		}
//gettimeofday(&time_end2, NULL);
//timeslot4 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);

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
//gettimeofday(&time_end2, NULL);
//timeslot5 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);

		// process buffered stream through clients
		for(int i = 0; (unsigned)i < clients.size(); i++)
		{
			while(clients[i]->msgbuf.size() > 1)
			{
				if(clients[i]->msgbuf.front()->is_end())
				{
//cout<<"\t\t\t\tdon't come here"<<endl;
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

//gettimeofday(&time_end2, NULL);
//timeslot6 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);

/*
		for(int i = 0; (unsigned)i < waitingclients.size(); i++)
		{
			int writeid;
			int countindex = -1;
			writeid = waitingclients[i]->get_writeid();

			writecount* thecount = NULL;

			for(int j = 0; (unsigned)j < writecounts.size(); j++)
			{
				if(writecounts[j]->get_id() == writeid)
				{
					thecount = writecounts[j];
					countindex = j;
				}
			}

			if(thecount == NULL)
			{
				// close the client
				close(waitingclients[i]->get_fd());
				delete waitingclients[i];
				waitingclients.erase(waitingclients.begin()+i);
				i--;
			}
			else
			{
				if(thecount->get_count() == 0) // the writing is already cleared
				{
					// close the client
					close(waitingclients[i]->get_fd());
					delete waitingclients[i];
					waitingclients.erase(waitingclients.begin()+i);
					i--;

					delete thecount;
					writecounts.erase(writecounts.begin()+countindex);
				}
				else if(thecount->get_count() > 0)
				{
					continue;
				}
				else
				{
					cout<<"[fileserver]Debugging: An abnormal write count."<<endl;
				}
			}
		}
*/

		// listen signal from cache server
		{
			int readbytes;
			readbytes = nbread(cacheserverfd, this->read_buf);
			if(readbytes > 0)
			{
				// do something here
			}
			else if(readbytes == 0)
			{
				cout<<"[fileserver]Connection from cache server disconnected."<<endl;
			}
		}
//gettimeofday(&time_end2, NULL);
//timeslot7 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);

		thecache->update_size();

//gettimeofday(&time_end2, NULL);
//timeslot8 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);


//	milli1 += timeslot1/1000;
//	milli2 += timeslot2/1000;
//	milli3 += timeslot3/1000;
//	milli4 += timeslot4/1000;
//	milli5 += timeslot5/1000;
//	milli6 += timeslot6/1000;
//	milli7 += timeslot7/1000;
//	milli8 += timeslot8/1000;

//	timeslot1 = timeslot1%1000;
//	timeslot2 = timeslot2%1000;
//	timeslot3 = timeslot3%1000;
//	timeslot4 = timeslot4%1000;
//	timeslot5 = timeslot5%1000;
//	timeslot6 = timeslot6%1000;
//	timeslot7 = timeslot7%1000;
//	timeslot8 = timeslot8%1000;

		// flush all the peer buffer every 1 msec
		gettimeofday(&time_end, NULL);
		if(1000*(time_end.tv_sec - time_start.tv_sec) + (time_end.tv_usec - time_start.tv_usec) > 1000) 
		{
			for(int i = 0; (unsigned)i < peers.size(); i++)
			{
				peers[i]->writebuffer.flush();
			}

			for(int i = 0; (unsigned)i < bridges.size(); i++)
			{
				if(bridges[i]->writebuffer != NULL)
					bridges[i]->writebuffer->flush();
			}

			gettimeofday(&time_start, NULL);
		}

//		if(1000*(time_end.tv_sec - time_start.tv_sec) + (time_end.tv_usec - time_start.tv_usec) > 1000) 
//		{
//			cout<<"Cache size["<<localhostname<<"]: "<<thecache->get_size()<<endl;
//			cout<<"------------------TIME SLOT------------------"<<endl;
//			cout<<"time slot1: "<<milli1<<" msec"<<endl;
//			cout<<"time slot2: "<<milli2<<" msec"<<endl;
//			cout<<"time slot3: "<<milli3<<" msec"<<endl;
//			cout<<"time slot4: "<<milli4<<" msec"<<endl;
//			cout<<"time slot5: "<<milli5<<" msec"<<endl;
//			cout<<"time slot6: "<<milli6<<" msec"<<endl;
//			cout<<"time slot7: "<<milli7<<" msec"<<endl;
//			cout<<"time slot8: "<<milli8<<" msec"<<endl;
//			cout<<"---------------------------------------------"<<endl;

//			cout<<"---------------------------------------------"<<endl;
//			cout<<"# clients: "<<clients.size()<<endl;
//			cout<<"# counts: "<<writecounts.size()<<endl;
//			cout<<"# bridges: "<<bridges.size()<<endl;
//			cout<<"---------------------------------------------"<<endl;
//			gettimeofday(&time_start, NULL);
//		}
	}
	return 0;
}

filepeer* fileserver::find_peer(string& address)
{
	for(int i = 0; (unsigned)i < peers.size(); i++)
	{
		if(peers[i]->get_address() == address)
			return peers[i];
	}

	cout<<"[fileserver]Debugging: No such a peer. in find_peer()"<<endl;
	return NULL;
}

filebridge* fileserver::find_bridge(int id)
{
	for(int i = 0; (unsigned)i < bridges.size(); i++)
	{
		if(bridges[i]->get_id() == id)
			return bridges[i];
	}

	cout<<"[fileserver]Debugging: No such a bridge. in find_bridge(), id: "<<id<<endl;
	return NULL;
}

bool fileserver::write_file(string fname, string& record)
{
	string fpath = DHT_PATH;
	int writefilefd = -1;
	int ret;
	fpath.append(fname);

	writefilefd = open(fpath.c_str(), O_APPEND|O_WRONLY|O_CREAT, 0644);

	if(writefilefd < 0)
		cout<<"filebridge]Opening write file failed"<<endl;

	record.append("\n");

	// memset(write_buf, 0, BUF_SIZE); <- memset may be not necessary
	strcpy(write_buf, record.c_str());
	ret = write(writefilefd, write_buf, record.length());

	if(ret < 0)
	{
		cout<<"[fileserver]Writing to write file failed"<<endl;
		close(writefilefd);
		return false;
	}
	else
	{
		close(writefilefd);
		return true;
	}
}

//writecount* fileserver::find_writecount(int id, int* idx)
//{
//	for(int i = 0; (unsigned)i < writecounts.size(); i++)
//	{
//		if(writecounts[i]->get_id() == id)
//		{
//			if(idx != NULL)
//				*idx = i;
//			return writecounts[i];
//		}
//	}
//
//	return NULL;
//}

#endif
