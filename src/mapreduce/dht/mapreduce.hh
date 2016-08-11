#ifndef __MAPREDUCE__
#define __MAPREDUCE__

#include <iostream>
#include <errno.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include <common/fileclient.hh>
#include <common/msgaggregator.hh>
#include <common/hash.hh>
#include <common/settings.hh>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/unistd.h>
#include <arpa/inet.h>

#include "../definitions.hh"

using namespace std;

// user functions
void init_mapreduce (int argc, char** argv);   // initialize mapreduce configure
void summ_mapreduce(); // summarize mapreduce configure
void set_mapper (void (*map_func) (string));
void set_reducer (void (*red_func) (string key));
bool is_nextvalue(); // return true if there is next value
bool is_nextrecord(); // return true if there is next value
string get_nextvalue(); // returns values in reduce function
bool get_nextinput (string& inputpath);   // process to next input for map role
string get_nextrecord(); // return true when successful, false when out of input record
bool get_nextkey (string* value);   // return true when successful, false when out of key value pair
void add_inputpath (string path);
void set_outputpath (string path);
void put_file (string path);
void get_file (string path);
char** get_argv (void);   // get user argv excepting passed pipe fd
void write_keyvalue (string key, string value);
void write_output (string record);   // function used in reduce function
void enable_Icache(); // function that enables intermediate cache
void set_nummapper (int num);   // sets number of mappers
void set_numreducer (int num);   // sets number of reducers

int get_argc (void);   // get user argc excepting passed pipe fd
int connect_to_server (char *host, unsigned short port);
int get_jobid();

mr_role role = JOB;
char *read_buf;
char *write_buf;

int argcount = -1;
char** argvalues = NULL;

// variables for job role
int port = -1;
int dhtport = -1;
int masterfd = -1;
int ipcfd = -1;
int jobid;
int nummap = 0;
int numreduce = 0;
int completed_map = 0;
int completed_reduce = 0;

bool master_is_set = false; // check if the configure file includes master address
bool isset_outputpath = false;
bool isset_mapper = false;
bool isset_reducer = false;
bool inside_map = false; // true if the code is inside map function
bool inside_reduce = false; // true if the code is inside reduce function
bool isIcache = false; // whether Icache option enabled

char *master_address;
vector<string> inputpaths; // list of input paths.

int currentpeer = -1;
vector<int> peerids; // list of peer ids on which the idata is located.
vector<int> numiblocks; // list of nummber of iblocks, element of which are mathced to the peerids(vector<int>)
//vector<string> nodelist; // list of slave node addresses
//ifstream input; // input file stream for get_record
//filetype inputtype = NOTOPENED; // type of the input data
//filetype outputtype = NOTOPENED; // type of the input data
string outputpath = "default_output";

// variables for task role
string jobdirpath;
int taskid;
int pipefd[2]; // pipe fd when the role is map task or reduce task
//DHTclient* dhtclient;
fileclient thefileclient;

// variables for map task
void (*mapfunction) (string inputpath);     // map function pointer
set<string> reported_keys;
set<string> unreported_keys;

// variables for reduce task
void (*reducefunction) (string key);     // reduce function pointer
string nextvalue;
string nextrecord;

bool is_nextval = false;
bool is_nextrec = false;


// WB
vector<int> threadids;
bool is_disk_write = true;


void init_mapreduce (int argc, char** argv)
{
  read_buf = (char*)malloc(BUF_SIZE);
  write_buf = (char*)malloc(BUF_SIZE);
  master_address = (char*)malloc(BUF_SIZE);
	int readbytes; // number of bytes read from pipe fd

	// check the arguments do determine the role
	if (argc > 1)     // check argc to avoid index out of bound
	{
		if (strncmp (argv[argc - 1], "MAP", 3) == 0)
		{
			role = MAP;
		}
		else if (strncmp (argv[argc - 1], "REDUCE", 6) == 0)
		{
			role = REDUCE;
		}
		else
		{
			role = JOB;
		}
	}
	else
	{
		role = JOB;
	}

	Settings setted;
	setted.load_settings();

	try
	{
		port = setted.port();
		dhtport = setted.dhtport();
		strcpy (master_address, setted.master_addr().c_str());
	}
	catch (exception& e)
	{
		cout << e.what() << endl;
	}






	/*
	   ifstream conf;
	   string token;
	   string confpath = LIB_PATH;
	   confpath.append ("setup.conf");
	   conf.open (confpath.c_str());
	   conf >> token;

	   while (!conf.eof())
	   {
	   if (token == "port")
	   {
	   conf >> token;
	   port = atoi (token.c_str());
	   }
	   else if (token == "dhtport")
	   {
	   conf >> token;
	   dhtport = atoi (token.c_str());
	   }
	   else if (token == "max_job")
	   {
	// ignore and just pass through this case
	conf >> token;
	}
	else if (token == "master_address")
	{
	conf >> token;
	strcpy (master_address, token.c_str());
	master_is_set = true;
	}
	else
	{
	cout << "Unknown configure record: " << token << endl;
	}

	conf >> token;
	}

	conf.close();
	*/


	if (role == JOB)     // when the role is job
	{
		// determine the argcount
		argcount = argc;
		masterfd = connect_to_server (master_address, port);

		if (masterfd <= 0)
		{
			cout << "Connecting to master failed" << endl;
			exit (1);
		}
		else
		{
			cout << "Connection to the master node successfully established" << endl;
		}

		// a blocking read "whoareyou" signal from master
		readbytes = nbread (masterfd, read_buf);

		if (readbytes == 0)     // connection closed
		{
			cout << "Connection to master is abnormally closed" << endl;
			cout << "Exiting..." << endl;
			exit (1);
		}
		else
		{
			if (strncmp (read_buf, "whoareyou", 9) == 0)
			{
				// respond to "whoareyou"
				memset (write_buf, 0, BUF_SIZE);
				strcpy (write_buf, "job");
				nbwrite (masterfd, write_buf);

				// blocking read of job id
				while (1)
				{
					readbytes = nbread (masterfd, read_buf);

					if (readbytes == 0)
					{
						cout << "[mapreduce]Connection from master abnormally close" << endl;
						break;
					}
					else if (readbytes < 0)
					{
						continue;
					}
					else     // reply arrived
					{
						break;
					}
				}

				// register the job id and proceed
				if (strncmp (read_buf, "jobid", 5) == 0)
				{
					char* token;
					token = strtok (read_buf, " ");   // token -> jobid
					token = strtok (NULL, " ");   // token -> job id(a number)
					// register the job id
					jobid = atoi (token);
				}
				else     // if the message is not the 'jobid'
				{
					cout << "[mapreduce]Debugging: protocol error in mapreduce" << endl;
				}

				cout << "[mapreduce]Job submitted(jobid: " << jobid << ")" << endl;
			}
			else
			{
				cout << "Undefined message from master node: " << read_buf << endl;
				cout << "Exiting..." << endl;
			}
		}

		// set master fd to be nonblocking to avoid deadlock
		fcntl (masterfd, F_SETFL, O_NONBLOCK);
	}
	else if (role == MAP)       // when the role is map task
	{
		// process pipe arguments and write id
		pipefd[0] = atoi (argv[argc - 3]); // read fd
		pipefd[1] = atoi (argv[argc - 2]); // write fd
		argcount = argc - 3;
		// request the task configuration
		memset (write_buf, 0, BUF_SIZE);
		strcpy (write_buf, "requestconf");
		nbwrite (pipefd[1], write_buf);
		// blocking read until the arrival of 'taskconf' message from master
		fcntl (pipefd[0], F_SETFL, fcntl (pipefd[0], F_GETFL) & ~O_NONBLOCK);
		readbytes = nbread (pipefd[0], read_buf);

		if (readbytes == 0)
		{
			cout << "[mapreduce]The connection from slave node is abnormally closed" << endl;
			exit (1);
		}

		char* token;
		token = strtok (read_buf, " ");   // token <- "taskconf"
		token = strtok (NULL, " ");   // token <- jobid
		jobid = atoi (token);
		stringstream jobidss;
		jobidss << ".job_";
		jobidss << jobid;
		jobidss << "_";
		jobdirpath = jobidss.str();
		token = strtok (NULL, " ");   // token <- taskid
		taskid = atoi (token);

		// read messages from slave until getting Einput
		while (1)
		{
			readbytes = nbread (pipefd[0], read_buf);

			if (readbytes == 0)
			{
				cout << "[mapreduce]Connection from slave is abnormally closed" << endl;
			}
			else if (readbytes < 0)
			{
				cout << "[mapreduce]A negative return from nbread with blocking read" << endl;
				continue;
			}
			else     // a message
			{
				if (strncmp (read_buf, "inputpath", 9) == 0)
				{
					token = strtok (read_buf, " ");   // token <- "inputpath"
					token = strtok (NULL, " ");   // first path

					while (token != NULL)
					{
						// add the input path to the task
						inputpaths.push_back (token);
						token = strtok (NULL, " ");
					}
				}
				else if (strncmp (read_buf, "Einput", 6) == 0)
				{
					// break the while loop
					break;
				}
				else
				{
					cout << "[mapreduce]Unexpected message order from slave" << endl;
				}
			}
		}

		fcntl (pipefd[0], F_SETFL, O_NONBLOCK);
	}
	else     // when the role is reduce
	{
		// process pipe arguments and write id
		pipefd[0] = atoi (argv[argc - 3]); // read fd
		pipefd[1] = atoi (argv[argc - 2]); // write fd
		argcount = argc - 3;
		// request the task configuration
		memset (write_buf, 0, BUF_SIZE);
		strcpy (write_buf, "requestconf");
		nbwrite (pipefd[1], write_buf);
		// blocking read until the arrival of 'taskconf' message from master
		fcntl (pipefd[0], F_SETFL, fcntl (pipefd[0], F_GETFL) & ~O_NONBLOCK);
		readbytes = nbread (pipefd[0], read_buf);

		if (readbytes == 0)
		{
			cout << "[mapreduce]The connection from slave node is abnormally closed" << endl;
			exit (1);
		}

		char* token;
		token = strtok (read_buf, " ");   // token <- "taskconf"
		token = strtok (NULL, " ");   // token <- jobid
		jobid = atoi (token);
		stringstream jobidss;
		jobidss << ".job_";
		jobidss << jobid;
		jobidss << "_";
		jobdirpath = jobidss.str();
		token = strtok (NULL, " ");   // token <- taskid
		taskid = atoi (token);

		// read messages from slave until getting Einput
		while (1)
		{
			readbytes = nbread (pipefd[0], read_buf);

			if (readbytes == 0)
			{
				cout << "[mapreduce]Connection from slave is abnormally closed" << endl;
			}
			else if (readbytes < 0)
			{
				cout << "[mapreduce]A negative return from nbread with blocking read" << endl;
				continue;
			}
			else     // a message
			{
				if (strncmp (read_buf, "inputpath", 9) == 0)
				{
					token = strtok (read_buf, " ");   // token <- "inputpath"
					token = strtok (NULL, " ");   // first peer id

					while (token != NULL)
					{
						// add the input path to the task
						peerids.push_back (atoi (token));
						token = strtok (NULL, " ");   // number of iblock
						numiblocks.push_back (atoi (token));
						token = strtok(NULL, " "); // thread id
						threadids.push_back(atoi(token));
						token = strtok (NULL, " ");   // next peer id
					}

					break;
				}
				else
				{
					cout << "[mapreduce]Unexpected message order from slave" << endl;
				}
			}
		}

		fcntl (pipefd[0], F_SETFL, O_NONBLOCK);
	}

	// parse user arguments
	argvalues = new char*[argcount];

	for (int i = 0; i < argcount; i++)   // copy argv into argvalues to get user argv
	{
		argvalues[i] = new char[strlen (argv[i]) + 1];
		strcpy (argvalues[i], argv[i]);
	}
}

void summ_mapreduce()
{
	int readbytes;

	// TODO: make sure that all configuration are done
	if (argcount == -1)     // mapreduce has not been initialized with init_mapreduce() func
	{
		cout << "Mapreduce has not been initialized" << endl;
		exit (1);
	}

	if (role == JOB)     // running job
	{
		if ( (nummap >= 0 && isset_mapper) || (numreduce >= 0 && isset_reducer))        // when neither mapper and reducer are activated
		{
			// send all necessary information to the master node
			string write_string = "jobconf ";
			stringstream ss;
			ss << "argcount ";
			ss << argcount;
			// parse the arguments
			ss << " argvalues";
			// find the program name and pass as 0th argument
			char* tmp = new char[strlen (argvalues[0]) + 1];
			string apath = MR_PATH;
			char* token = NULL;
			char* next_token = NULL;
			strcpy (tmp, argvalues[0]);
			next_token = strtok (tmp, "/");

			while (next_token != NULL)
			{
				token = next_token;
				next_token = strtok (NULL, "/");
			}

			ss << " ";
			apath.append ("app/");
			apath.append (token);   // token <- the program name
			ss << apath;
			delete[] tmp;

			for (int i = 1; i < argcount; i++)
			{
				ss << " ";
				ss << argvalues[i];
			}

			ss << " nummap ";
			ss << nummap;
			ss << " numreduce ";
			ss << numreduce;
			write_string.append (ss.str());
			memset (write_buf, 0, BUF_SIZE);
			strcpy (write_buf, write_string.c_str());
			nbwrite (masterfd, write_buf);
			// prepare inputpath message
			stringstream ss2;
			string message;
			int iter = 0;
			ss2 << "inputpath ";
			ss2 << inputpaths.size();
			message = ss2.str();

			while ( (unsigned) iter < inputpaths.size())
			{
				if (message.length() + inputpaths[iter].length() + 2 < BUF_SIZE)     // +1 for white space, +1 for zero packet
				{
					message.append (" ");
					message.append (inputpaths[iter]);
				}
				else
				{
					memset (write_buf, 0, BUF_SIZE);
					strcpy (write_buf, message.c_str());
					nbwrite (masterfd, write_buf);
					message = inputpaths[iter];
				}

				iter++;
			}

			// send last message
			memset (write_buf, 0, BUF_SIZE);
			strcpy (write_buf, message.c_str());
			nbwrite (masterfd, write_buf);
		}

		// blocking read from master until "complete" receiving message
		fcntl (masterfd, F_SETFL, fcntl (masterfd, F_GETFL) & ~O_NONBLOCK);

		while (1)
		{
			readbytes = nbread (masterfd, read_buf);

			if (readbytes == 0)     // master abnormally terminated
			{
				// TODO: Terminate the job properly
				while (close (pipefd[0]) < 0)
				{
					cout << "close failed" << endl;
					// sleeps for 1 milli seconds
					usleep (100000);
				}

				while (close (pipefd[1]) < 0)
				{
					cout << "close failed" << endl;
					// sleeps for 1 milli seconds
					usleep (100000);
				}

				cout << "[mapreduce]Connection to master abnormally closed" << endl;
				exit (0);
			}
			else
			{
				if (strncmp (read_buf, "complete", 8) == 0)       // "complete" message received
				{
					cout << "[mapreduce]Job " << jobid << " is successfully completed" << endl;
					break;
				}
				else if (strncmp (read_buf, "mapcomplete", 11) == 0)
				{
					cout << "[mapreduce]Map tasks are completed" << endl;
				}
			}
		}

		exit (0);
	}
	else if (role == MAP)       // map task
	{
		// check whether no map or reduce function is running
		if (inside_map || inside_reduce)
		{
			cout << "[mapreduce]Debugging: The map or reduce function is called from the map or reduce function." << endl;
		}

		// run the mapfunction until input all inputs are processed
		thefileclient.connect_to_server();

		if (isset_mapper)
		{
			string inputpath;

			while (get_nextinput (inputpath))
			{
				inside_map = true;
				(*mapfunction) (inputpath);
				inside_map = false;
				// report generated keys to slave node
				/*
				   while(!unreported_keys.empty())
				   {
				   string key = *unreported_keys.begin();
				//cout<<"[mapreduce]Debugging: key emitted: "<<key<<endl;
				unreported_keys.erase(*unreported_keys.begin());
				reported_keys.insert(key);

				// send 'key' meesage to the slave node
				keybuffer.add_record(key);
				}
				*/
			}

			//cout<<"[mapreduce]Finished all map tasks..."<<endl;
		}

		set<int> peerids;
		thefileclient.wait_write (&peerids);
		string message;
		stringstream ss;
		ss << "complete";

		for (set<int>::iterator it = peerids.begin(); it != peerids.end(); it++)
		{
			ss << " ";
			ss << *it;
		}

		message = ss.str();
		// send complete message with peerds
		memset (write_buf, 0, BUF_SIZE);
		strcpy (write_buf, message.c_str());
		nbwrite (pipefd[1], write_buf);
		//cout<<"[mapreduce]Complete message sent..."<<endl;
		// blocking read until the 'terminate' message
		fcntl (pipefd[0], F_SETFL, fcntl (pipefd[0], F_GETFL) & ~O_NONBLOCK);

		while (1)
		{
			readbytes = nbread (pipefd[0], read_buf);

			if (readbytes == 0)     // pipe fd was closed abnormally
			{
				// TODO: Terminate the task properly
				cout << "[mapreduce]Connection from master abnormally closed" << endl;
				exit (0);
			}
			else if (readbytes > 0)
			{
				if (strncmp (read_buf, "terminate", 9) == 0)
				{
					// cout<<"[mapreduce]Map task is successfully completed"<<endl;
					// terminate successfully
					while (close (pipefd[0]) < 0)
					{
						cout << "[mapreduce]close failed" << endl;
						// sleeps for 1 milli seconds
						usleep (1000);
					}

					while (close (pipefd[1]) < 0)
					{
						cout << "[mapreduce]close failed" << endl;
						// sleeps for 1 milli seconds
						usleep (1000);
					}

					exit (0);
				}
				else   // all other messages are ignored
				{
					continue;
				}
			}
			else
			{
				usleep (1000);
			}

			// sleeps for 0.0001 seconds. change this if necessary
			// usleep(100000);
		}

		fcntl (pipefd[0], F_SETFL, O_NONBLOCK);
	}
	else     // reduce task
	{
		// check whether no map or reduce function is running
		if (inside_map || inside_reduce)
		{
			cout << "[mapreduce]Debugging: The map or reduce function is called from the map or reduce function." << endl;
		}

		thefileclient.connect_to_server();

		// run the reduce functions until all key are processed
		if (isset_reducer)
		{
			// request first key
			currentpeer = 0;
			string ipath;
			stringstream ss;
			ss << jobid;
			ss << " ";
			ss << peerids[currentpeer];
			ss << " ";
			ss << threadids[currentpeer];
			ss << " ";
			ss << numiblocks[currentpeer];
			ipath = ss.str();
			thefileclient.read_request (ipath, INTERMEDIATE);
			currentpeer++;
			// run reducers
			string key;

			while (get_nextkey (&key))
			{
				inside_reduce = true;
				(*reducefunction) (key);
				inside_reduce = false;
			}
		}

		thefileclient.wait_write (NULL);
		//set<int> peerids;
		//thefileclient.wait_write (&peerids);
		
		// send complete message
		memset (write_buf, 0, BUF_SIZE);
		strcpy (write_buf, "complete");
		nbwrite (pipefd[1], write_buf);
		// blocking read until 'terminate' message arrive
		fcntl (pipefd[0], F_SETFL, fcntl (pipefd[0], F_GETFL) & ~O_NONBLOCK);

		while (1)
		{
			readbytes = nbread (pipefd[0], read_buf);

			if (readbytes == 0)     // pipe fd was closed abnormally
			{
				cout << "the reduce task is gone" << endl;
				exit (0);
			}
			else if (readbytes > 0)
			{
				if (strncmp (read_buf, "terminate", 9) == 0)
				{
					// cout<<"[mapreduce]Reduce task is successfully completed"<<endl; // <- this message will be printed in the slave process side
					// terminate successfully
					exit (0);
				}
				else   // all other messages are ignored
				{
					continue;
				}
			}

			// sleeps for 0.0001 seconds. change this if necessary
			// usleep(100000);
		}

		fcntl (pipefd[0], F_SETFL, O_NONBLOCK);
	}
}

int get_argc (void)
{
	return argcount;
}

char** get_argv (void)
{
	return argvalues;
}
void set_mapper (void (*map_func) (string))
{
	isset_mapper = true;
	mapfunction = map_func;
}

void set_reducer (void (*red_func) (string key))
{
	isset_reducer = true;
	reducefunction = red_func;
}

void add_inputpath (string path)     // the path is relative path to DHT_PATH
{
	if (role == JOB)
	{
		inputpaths.push_back (path);
	}
	else if (role == MAP)
	{
		// do nothing
	}
	else     // role is reduce
	{
		// do nothing
	}
}

void set_outputpath (string path)     // this user function can be used in anywhere but before initialization
{
	isset_outputpath = true;
	outputpath = path;
}

// map/reduce task calls this function for ipc socket 
int connect_to_server (char *host, unsigned short port)
{
	int clientfd;
	struct sockaddr_in serveraddr;
	struct hostent *hp;
	// SOCK_STREAM -> tcp
	clientfd = socket (AF_INET, SOCK_STREAM, 0);

	if (clientfd < 0)
	{
		cout << "[mapreduce]Openning socket failed" << endl;
		exit (1);
	}

	hp = gethostbyname (host);

	if (hp == NULL)
	{
		cout << "[mapreduce]Cannot find host by host name" << endl;
		return -1;
	}

	memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
	serveraddr.sin_family = AF_INET;
	memcpy (&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
	serveraddr.sin_port = htons (port);
	connect (clientfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr));
	return clientfd;
}

void write_keyvalue (string key, string value)
{
	// check if thie function is called inside the map function
	if (inside_map)
	{
		thefileclient.write_record (key, value, INTERMEDIATE);
	}
	else
	{
		cout << "[mapreduce]Warning: the write_keyvalue() function is being used outside the map function" << endl;
	}
}

bool get_nextinput (string& inputpath)     // internal function to process next input file
{
	if (inputpaths.size() == 0)     // no more input
	{
		return false;
	}
	else
	{
		// request the location of the input to the dht server
		// NOTE: for this version, we can simply call hash function
		// rather than requesting to dht server
		/*
		   string address;
		   string inputname = inputpaths.back();

		// use read_buf as temporal buffer for hash function
		memset(read_buf, 0, BUF_SIZE);
		strcpy(read_buf, inputname.c_str());

		uint32_t hashvalue = h(read_buf, HASHLENGTH);
		hashvalue = hashvalue%nodelist.size();

		address = nodelist[hashvalue];
		*/
		bool readsuccess = false;
		bool cachehit = false;
		string request;
		stringstream ss;
		// configure request message of write buffer
		ss << argvalues[0]; // app/[app name]  <- for Icache
		ss << " ";
		ss << jobid;
		ss << " ";
		ss << inputpaths.back();
		request = ss.str();
		// request <- app/[app name] [job directory path] [input path]
		cachehit = thefileclient.read_request (request, RAW);

		while (cachehit)
		{
			// forward to next input
			inputpaths.pop_back();

			// return false if inputpaths is empty
			if (inputpaths.size() == 0)
			{
				return false;
			}

			stringstream ss1;
			// configure request message of write buffer
			ss1 << argvalues[0]; // app/[app name]  <- for Icache
			ss1 << " ";
			ss1 << jobid;
			ss1 << " ";
			ss1 << inputpaths.back();
			request = ss1.str();
			// request <- app/[app name] [input path]
			cachehit = thefileclient.read_request (request, RAW);
		}

		thefileclient.configure_buffer_initial (jobdirpath, argvalues[0], inputpaths.back(), isIcache);
		inputpath = inputpaths.back();
		inputpaths.pop_back();

		// pre-process first record

		readsuccess = thefileclient.read_record (&nextrecord);

		if (readsuccess)
		{
			is_nextrec = true;
		}
		else
		{
			is_nextrec = false;
		}

		return true;
	}
}

string get_next64mb()   // a user function for the map
{
	if (inside_map)
	{
		bool readsuccess = false;
		string ret = nextrecord;
		readsuccess = thefileclient.read_64mb (&nextrecord);

		if (readsuccess)
		{
			is_nextrec = true;
		}
		else
		{
			is_nextrec = false;
		}

		return ret;
	}
	else
	{
		cout << "[mapreduce]Warning: the get_nextrecord() function is being used outside the map function" << endl;
		return "";
	}
}

string get_nextrecord()   // a user function for the map
{
	if (inside_map)
	{
		bool readsuccess = false;
		string ret = nextrecord;
		readsuccess = thefileclient.read_record (&nextrecord);

		if (readsuccess)
		{
			is_nextrec = true;
		}
		else
		{
			is_nextrec = false;
		}

		return ret;
	}
	else
	{
		cout << "[mapreduce]Warning: the get_nextrecord() function is being used outside the map function" << endl;
		return "";
	}
}

bool is_nextrecord()
{
	if (inside_map)
	{
		return is_nextrec;
	}
	else
	{
		cout << "[mapreduce]Warning: the is_nextrecord() function is being used outside the map function" << endl;
		return false;
	}
}

bool get_nextkey (string* key)     // internal function for the reduce
{
	if (is_nextval)     // if there are some remaining values of this key
	{
		// flush all values of this key
		bool readsuccess = true;

		while (readsuccess)
		{
			readsuccess = thefileclient.read_record (&nextvalue);
		}
	}

	// get next key
	bool readsuccess = thefileclient.read_record (key);

	if (readsuccess)     // next key exist
	{
		readsuccess = thefileclient.read_record (&nextvalue);

		if (readsuccess)
		{
			is_nextval = true;
		}
		else
		{
			is_nextval = false;
			cout << "[mapreduce]Unexpected respond after read_request(1)" << endl;
			cout << "[mapreduce]key received: " << *key << endl;
			return false;
		}

		return true;
	}
	else     // no next key in this node
	{
		if ( (unsigned) currentpeer < peerids.size())      // another peer to go
		{
			// request to next peer
			string ipath;
			stringstream ss;
			ss << jobid;
			ss << " ";
			ss << peerids[currentpeer];
			ss << " ";
			ss << numiblocks[currentpeer];
			ipath = ss.str();
			thefileclient.read_request (ipath, INTERMEDIATE);
			currentpeer++;
			// receive first key
			readsuccess = thefileclient.read_record (key);

			if (readsuccess)     // first key exist
			{
				readsuccess = thefileclient.read_record (&nextvalue);

				if (readsuccess)
				{
					is_nextval = true;
				}
				else
				{
					cout << "[mapreduce]Unexpected respond after read_request(2)" << endl;
				}
			}
			else     // no first key
			{
				cout << "[mapreduce]Unexpected respond after read_request(3)" << endl;
			}

			return true;
		}
		else
		{
			return false;
		}
	}
}

bool is_nextvalue()   // returns true if there is next value
{
	// check if this function is called inside the reduce function
	if (inside_reduce)
	{
		return is_nextval;
	}
	else
	{
		cout << "[mapreduce]Warning: the is_nextvalue() function is being used outside the reduce function" << endl;
		return false;
	}
}

string get_nextvalue()   // returns values in reduce function
{
	// check if this function is called inside the reduce function
	if (inside_reduce)
	{
		string ret = nextvalue;
		bool readsuccess = false;

		if (!is_nextval)     // no next value
		{
			return "";
		}
		else
		{
			readsuccess = thefileclient.read_record (&nextvalue);   // key value record

			// pre-process first record
			if (readsuccess)
			{
				is_nextval = true;
			}
			else
			{
				is_nextval = false;
			}

			return ret;
		}
	}
	else
	{
		cout << "[mapreduce]Warning: the get_nextvalue() function is being used outside the reduce function" << endl;
		return "";
	}
}

void write_output (string record)     // this user function can be used anywhere but after initialization
{
	if (!isset_outputpath)
	{
		stringstream ss;
		ss << "job_";
		ss << jobid;
		ss << ".txt";
		outputpath = ss.str();
	}

	thefileclient.write_record (outputpath, record, OUTPUT);
}

void set_nummapper (int num)     // sets number of mappers
{
	nummap = num;
}

void set_numreducer (int num)     // sets number of reducers
{
	numreduce = num;
}

int get_jobid()
{
	return jobid;
}

void enable_Icache()   // function that enables intermediate cache
{
	isIcache = true;
}

void DisableDiskWrite() {
	is_disk_write = false;
}

void WriteOutputCache(string record) {  // writing to a local cache (why? because this is writing to the input cache
	if (!isset_outputpath)
	{
		stringstream ss;
		ss << "job_";
		ss << jobid;
		ss << ".txt";
		outputpath = ss.str();
	}   // as in input data caching, we need a file name. 

	thefileclient.WriteRecordCache(outputpath, record, is_disk_write);
}

#endif
