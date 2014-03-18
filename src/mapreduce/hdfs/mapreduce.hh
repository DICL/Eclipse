#ifndef _MAPREDUCE_
#define _MAPREDUCE_

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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <hdfs.h>

#include "../definitions.hh"

#define HDFS_HOST "ravenleader"
#define HDFS_PORT 9000

using namespace std;

// user functions
void init_mapreduce(int argc, char** argv); // initialize mapreduce configure
void summ_mapreduce(); // summarize mapreduce configure
void set_mapper(void (*map_func) ());
void set_reducer(void (*red_func) (string key));
bool is_nextvalue(); // return true if there is next value
bool is_nextrecord(); // return true if there is next value
string get_nextvalue(); // returns values in reduce function
bool get_nextinput(); // process to next input for map role
string get_nextrecord(); // return true when successful, false when out of input record
bool get_nextkey(string* value); // return true when successful, false when out of key value pair
void add_inputpath(string path);
void set_outputpath(string path);
char** get_argv(void); // get user argv excepting passed pipe fd
void write_keyvalue(string key, string value);
void write_output(string record); // function used in reduce function

int writeoutfile(hdfsFile* hdfsFile, string data); // write to the task/job result file
bool get_record(string* record); // return true when successful, false when out of input record
bool get_nextkey(string* value); // return true when successful, false when out of key value pair
int get_argc(void); // get user argc excepting passed pipe fd
void report_key(int index);
int connect_to_server(char *host, unsigned short port);
int get_jobid();
bool hdfs_getline(hdfsFile* file, string* ret);

mr_role role = JOB;
char read_buf[BUF_SIZE]; // read buffer for pipe
char write_buf[BUF_SIZE]; // write buffer for pipe

int argcount = -1;
char** argvalues = NULL;

// variables for job role
int port = -1;
int dhtport = -1;
int masterfd = -1;
int jobid;
int nummap = 0;
int numreduce = 0;
int completed_map = 0;
int completed_reduce = 0;
bool master_is_set = false; // check if the configure file includes master address
bool isset_mapper = false;
bool isset_reducer = false;
bool inside_map = false; // true if the code is inside map function 
bool inside_reduce = false; // true if the code is inside reduce function 
char master_address[BUF_SIZE];
vector<string> inputpaths; // list of input paths.
hdfsFile input; // input file stream for get_record
string outputpath = "default_output";

// variables for task role

hdfsFS hadoopfs;
string jobdirpath;
int taskid;
int pipefd[2]; // pipe fd when the role is map task or reduce task

// variables for map task
void (*mapfunction) (); // map function pointer
set<string> reported_keys;
set<string> unreported_keys;

// variables for reduce task
void (*reducefunction) (string key); // reduce function pointer
string nextvalue;
string nextrecord;
bool is_nextval = false;
bool is_nextrec = false;

void init_mapreduce(int argc, char** argv)
{
	// connect to hdfs server
	hadoopfs = hdfsConnect(HDFS_HOST, HDFS_PORT);

	int readbytes; // number of bytes read from pipe fd

	// check the arguments do determine the role
	if(argc>1) // check argc to avoid index out of bound
	{
		if(strncmp(argv[argc-1], "MAP", 3) == 0)
			role = MAP;
		else if(strncmp(argv[argc-1], "REDUCE", 6) == 0)
			role = REDUCE;
		else
			role = JOB;
	}
	else
	{
		role = JOB;
	}

	if(role == JOB) // when the role is job
	{
		// determine the argcount
		argcount = argc;

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
				conf>>token;
				port = atoi(token.c_str());
			}
			else if(token == "dhtport")
			{
				conf>>token;
				dhtport = atoi(token.c_str());
			}
			else if(token == "max_job")
			{
				// ignore and just pass throught this case
				conf>>token;
			}
			else if(token == "num_slave")
			{
				// ignore and just pass throught this case
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
				cout<<"Unknown configure record: "<<token<<endl;
			}
			conf>>token;
		}
		conf.close();

		// verify initialization
		if(port == -1)
		{
			cout<<"Port should be specified in the setup.conf"<<endl;
			exit(1);
		}
		if(master_is_set == false)
		{
			cout<<"Master_address should be specified in the setup.conf"<<endl;
			exit(1);
		}

		masterfd = connect_to_server(master_address, port);
		if(masterfd<0)
		{
			cout<<"Connecting to master failed"<<endl;
			exit(1);
		}
		else
		{
			cout<<"Connection to the mater node successfully established"<<endl;
		}

		// read "whoareyou" signal from master
		readbytes = nbread(masterfd, read_buf);

		if(readbytes == 0) // connection closed
		{
			cout<<"Connection to master is abnormally closed"<<endl;
			cout<<"Exiting..."<<endl;
			exit(1);
		}
		else
		{
			if(strncmp(read_buf, "whoareyou", 9) == 0)
			{
				// respond to "whoareyou"
				memset(write_buf, 0, BUF_SIZE);
				strcpy(write_buf, "job");
				nbwrite(masterfd, write_buf);

				// blocking read of job id
				while(1)
				{
					readbytes = nbread(masterfd, read_buf);
					if(readbytes == 0)
					{
						cout<<"[mapreduce]Connection from master abnormally close"<<endl;
						break;
					}
					else if(readbytes < 0)
					{
						// sleep for 0.0001 second. change this if necessary
						// usleep(100);
					}
					else // reply arived
					{
						break;
					}
				}

				// register the job id and proceed
				if(strncmp(read_buf, "jobid", 5) == 0)
				{
					char* token;
					token = strtok(read_buf, " "); // token -> jobid
					token = strtok(NULL, " "); // token -> job id(a number)

					// register the job id
					jobid = atoi(token);
				}
				else // if the message is not the 'jobid'
				{
					cout<<"[mapreduce]Debugging: protocol error in mapreduce"<<endl;
				}
				cout<<"[mapreduce]Debugging: Job id is: "<<jobid<<endl;
			}
			else
			{
				cout<<"Undefined message from master node: "<<read_buf<<endl;
				cout<<"Exiting..."<<endl;
				exit(1);
			}
		}

		// set master fd to be nonblocking to avoid deadlock
		fcntl(masterfd, F_SETFL, O_NONBLOCK);

		// make lock directories for the job
		int pid;
		string lpath = MR_PATH;
		stringstream ljobidss;

		// pass job id to the string stream
		ljobidss<<".job_";
		ljobidss<<jobid;
		ljobidss<<"/";
		jobdirpath = ljobidss.str();
		lpath.append(jobdirpath);

		// fork new process and give command mkdir to the child process
		pid = fork();
		if(pid == 0) // child process
		{
			// parse the arguments to make the job directory
			char** argv;
			argv = new char*[3];
			argv[0] = new char[6];
			strcpy(argv[0], "mkdir");
			argv[1] = new char[lpath.length()+1];
			strcpy(argv[1], lpath.c_str());
			argv[2] = NULL;
			
			// launch the mkdir program
			execvp(argv[0], argv);
		}
		else // parent side
		{
			// do nothing
		}

		// make directories for the job
		string apath = HDMR_PATH;
		stringstream jobidss;

		// pass job id to the string stream
		jobidss<<".job_";
		jobidss<<jobid;
		jobidss<<"/";
		jobdirpath = jobidss.str();
		apath.append(jobdirpath);

		hdfsCreateDirectory(hadoopfs, apath.c_str());
	}
	else // when the role is map task or reduce task
	{
		int readcount = 0;
		pipefd[0] = atoi(argv[argc-3]); // read fd
		pipefd[1] = atoi(argv[argc-2]); // write fd
		argcount = argc - 3;

		// request the task configuration
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, "requestconf");
		nbwrite(pipefd[1], write_buf);

		// blocking read until the arrival of 'taskconf' message from master
		fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL) & ~O_NONBLOCK);
		readbytes = nbread(pipefd[0], read_buf);

		if(readbytes == 0)
		{
			cout<<"[mapreduce]the connection from slave node is abnormally closed"<<endl;
			exit(1);
		}
		fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

		// parse the task configure
		char* token;
		token = strtok(read_buf, " "); // token <- taskconf

		// check the message protocol
		if(strncmp(token, "taskconf", 8) != 0)
		{
			cout<<"[mapreduce]Debugging: The message protocol has problem"<<endl;
		}
		else
		{
			while(token != NULL)
			{
				if(strncmp(token, "jobid", 5) == 0)
				{
					// register job id and set job directory path
					stringstream jobidss;

					token = strtok(NULL, " "); // token -> job id
					jobid = atoi(token);

					jobidss<<".job_";
					jobidss<<jobid;
					jobidss<<"/";
					jobdirpath = jobidss.str();

				}
				else if(strncmp(token, "taskid", 6) == 0)
				{
					token = strtok(NULL, " "); // token -> taskid
					taskid = atoi(token);
				}
				else if(strncmp(token, "inputpaths", 10) == 0)
				{
					int numpath;
					token = strtok(NULL, " "); // token -> number of input paths
					numpath = atoi(token);
					for(int i=0;i<numpath;i++)
					{
						token = strtok(NULL, " ");
						inputpaths.push_back(token);
					}
				}

				// process next configure
				token = strtok(NULL, " ");
			}
		}
	}

	// parse user arguments
	argvalues = new char*[argcount];
	for(int i=0; i<argcount; i++) // copy argv into argvalues to get user argv
	{
		argvalues[i] = new char[strlen(argv[i])+1];
		strcpy(argvalues[i], argv[i]);
	}
}

void summ_mapreduce()
{
	int readbytes;
	// TODO: make sure that all configuration are done

	if(argcount == -1) // mapreduce has not been initialized with init_mapreduce() func
	{
		cout<<"Mapreduce has not been initialized"<<endl;
		exit(1);
	}

	if(role == JOB) // running job
	{
		if((nummap >= 0 && isset_mapper) || (numreduce >= 0 && isset_reducer)) // when neither mapper and reducer are activated
		{
			// TODO: manage all things if the role is the job

			// send all necessary information to the master node
			string write_string = "jobconf";
			stringstream ss;

			// TODO: deal with the case when number of characters exceeds BUF_SIZE
			ss<<" inputpath ";
			ss<<inputpaths.size();
			for(int i=0;i<inputpaths.size();i++)
			{
				ss<<" ";
				ss<<inputpaths[i];
			}

			ss<<" argcount ";
			ss<<argcount;

			// parse the arguments
			ss<<" argvalues";

			// find the program name and pass as 0th argument
			char* tmp = new char[strlen(argvalues[0])+1];
			string apath = MR_PATH;
			char* token;
			char* next_token;

			strcpy(tmp, argvalues[0]);
			next_token = strtok(tmp, "/");

			while(next_token != NULL)
			{
				token = next_token;
				next_token = strtok(NULL, "/");
			}

			ss<<" ";
			apath.append("app/");
			apath.append(token); // token <- the program name
			ss<<apath;

			delete[] tmp;

			for(int i=1;i<argcount;i++)
			{
				ss<<" ";
				ss<<argvalues[i];
			}

			write_string.append(ss.str());
			memset(write_buf, 0, BUF_SIZE);
			strcpy(write_buf, write_string.c_str());
			nbwrite(masterfd, write_buf);
		}

		// blocking read from master until "complete" receiving message
		fcntl(masterfd, F_SETFL, fcntl(masterfd, F_GETFL) & ~O_NONBLOCK);
		while(1)
		{
			readbytes = nbread(masterfd, read_buf);
			if(readbytes == 0) // master abnormally terminated
			{
				// TODO: Terminate the job properly
				exit(0);
			}
			else
			{
				if(strncmp(read_buf, "complete", 8) == 0) // "complete" message received
				{
					cout<<"[mapreduce]Job is successfully completed"<<endl;
					break;
				}
				else if(strncmp(read_buf, "mapcomplete", 11) == 0)
				{
					cout<<"[mapreduce]Map tasks are completed"<<endl;
					cout<<"[mapreduce]Now reduce tasks are launched"<<endl;
					continue;
				}
				else // all other messages are ignored
					continue;
			}
		}
		fcntl(masterfd, F_SETFL, O_NONBLOCK);

		// remove the job directory
		string apath = HDMR_PATH;
		apath.append(jobdirpath);
		hdfsDelete(hadoopfs, apath.c_str(), 1);

		// remove the lock directory
		int pid;
		pid = fork();
		if(pid == 0) // child process
		{
			string apath;
			char** argv = new char*[4];

			argv[0] = new char[3];
			strcpy(argv[0], "rm");

			argv[1] = new char[4];
			strcpy(argv[1], "-rf");

			apath = MR_PATH;
			apath.append(jobdirpath);
			argv[2] = new char[apath.length()+1];
			strcpy(argv[2], apath.c_str());

			argv[3] = NULL;

			execvp(argv[0], argv);
		}
		else if(pid < 0)
		{
			cout<<"[mapreduce]Debugging: forking failed"<<endl;
		}
		else // parent process
		{
			int status;
			waitpid(pid, &status, 0);
		}

		// disonnect from hdfs server and close the hdfs file
		hdfsDisconnect(hadoopfs);

		exit(0);
	}
	else if(role == MAP) // map task
	{
		// check whether no map or reduce function is running
		if(inside_map || inside_reduce)
		{
			cout<<"[mapreduce]Debugging: The map or reduce function is called from the map or reduce function."<<endl;
		}


		// run the mapfunction until input all inputs are processed
		string record;

		if(isset_mapper)
		{
			while(get_nextinput())
			{
				inside_map = true;
				(*mapfunction)();
				inside_map = false;

				// report generated keys to slave node
				while(!unreported_keys.empty())
				{
					string key = *unreported_keys.begin();
					string keystr = "key ";
					keystr.append(key);
cout<<"[mapreduce]Debugging: key emitted: "<<key<<endl;
					unreported_keys.erase(*unreported_keys.begin());
					reported_keys.insert(key);

					// send 'key' meesage to the slave node
					memset(write_buf, 0, BUF_CUT);
					strcpy(write_buf, keystr.c_str());
					nbwrite(pipefd[1], write_buf);
				}
			}
		}

		// send complete message
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, "complete");
		nbwrite(pipefd[1], write_buf);

		// blocking read until the 'terminate' message
		fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL) & ~O_NONBLOCK);
		while(1)
		{
			readbytes = nbread(pipefd[0], read_buf);
			if(readbytes == 0) // pipe fd was closed abnormally
			{
				// TODO: Terminate the task properly
				hdfsDisconnect(hadoopfs);
				exit(0);
			}
			else if(readbytes > 0)
			{
				if(strncmp(read_buf, "terminate", 9) == 0)
				{
					//cout<<"[mapreduce]Map task is successfully completed"<<endl;

					// clear task
					hdfsDisconnect(hadoopfs);

					// terminate successfully
					exit(0);
				}
				else // all other messages are ignored
					continue;
			}

			// sleeps for 0.0001 seconds. change this if necessary
			// usleep(100);
		}
		fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
	}
	else // reduce task
	{
		// check whether no map or reduce function is running
		if(inside_map || inside_reduce)
		{
			cout<<"[mapreduce]Debugging: The map or reduce function is called from the map or reduce function."<<endl;
		}

		// run the reduce functions until all key are processed
		if(isset_reducer)
		{
			string key;
			while(get_nextkey(&key))
			{
				inside_reduce = true;
				(*reducefunction)(key);
				inside_reduce = false;
			}
		}

		// send complete message
		memset(write_buf, 0, BUF_SIZE);
		strcpy(write_buf, "complete");
		nbwrite(pipefd[1], write_buf);

		// blocking read until 'terminate' message arrive
		fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL) & ~O_NONBLOCK);
		while(1)
		{
			readbytes = nbread(pipefd[0], read_buf);
			if(readbytes == 0) // pipe fd was closed abnormally
			{
				// TODO: Terminate the task properly
				cout<<"the reduce task is gone"<<endl;
				hdfsDisconnect(hadoopfs);
				exit(0);
			}
			else if(readbytes > 0)
			{
				if(strncmp(read_buf, "terminate", 9) == 0)
				{
					//					cout<<"[mapreduce]Reduce task is successfully completed"<<endl; // <- this message will be printed in the slave process side

					// clear task
					hdfsDisconnect(hadoopfs);
					// terminate successfully
					exit(0);
				}
				else // all other messages are ignored
					continue;
			}

			// sleeps for 0.0001 seconds. change this if necessary
			// usleep(100);
		}
		fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
	}
}

int get_argc(void)
{
	return argcount;
}

char** get_argv(void)
{
	return argvalues;
}
void set_mapper(void (*map_func) ())
{
	isset_mapper = true;
	mapfunction = map_func;
}

void set_reducer(void (*red_func) (string key))
{
	isset_reducer = true;
	reducefunction = red_func;
}

void add_inputpath(string path) // the path is relative path to MR_PATH
{
	if(role == JOB)
	{
		inputpaths.push_back(path);
	}
	else if(role == MAP)
	{
		// do nothing
	}
	else // role is reduce
	{
		// do nothing
	}
}

void set_outputpath(string path) // this user function can be used in anywhere but after initialization
{
	outputpath = path;
}

int connect_to_server(char *host, unsigned short port)
{
	int clientfd;
	struct sockaddr_in serveraddr;
	struct hostent *hp;

	// SOCK_STREAM -> tcp
	clientfd = socket(AF_INET, SOCK_STREAM, 0);
	if(clientfd<0)
	{
		cout<<"[mapreduce]Openning socket failed"<<endl;
		exit(1);
	}

	hp = gethostbyname(host);

	if (hp == NULL)
	{
		cout<<"[mapreduce]Cannot find host by host name"<<endl;
		return -1;
	}

	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
	return clientfd;
}

void write_keyvalue(string key, string value)
{
	// check if thie function is called inside the map function
	if(inside_map)
	{
		if(reported_keys.find(key) == reported_keys.end()
				&& unreported_keys.find(key) == unreported_keys.end())
		{
			unreported_keys.insert(key);

			// send 'key' message to the slave node
			string key = *unreported_keys.begin();
			string keystr = "key ";
			keystr.append(key);
			cout<<"[mapreduce]Debugging: key emitted: "<<key<<endl;
			unreported_keys.erase(*unreported_keys.begin());
			reported_keys.insert(key);

			// send 'key' meesage to the slave node
			memset(write_buf, 0, BUF_SIZE);
			strcpy(write_buf, keystr.c_str());
			nbwrite(pipefd[1], write_buf);
		}

		// path of the key file
		string keypath = HDMR_PATH;
		string keypath2 = MR_PATH;

		keypath.append(jobdirpath);
		keypath2.append(jobdirpath);
		keypath.append(key);
		keypath2.append(key);

		// result string
		string rst = key;
		rst.append(" ");
		rst.append(value);

		int fd = open(keypath2.c_str(), O_APPEND|O_SYNC|O_WRONLY|O_CREAT, 0644);
		struct flock alock;
		struct flock ulock;

		// set lock
		alock.l_type = F_WRLCK;
		alock.l_start = 0;
		alock.l_whence = SEEK_SET;
		alock.l_len = 0;

		//set unlock
		ulock.l_type = F_UNLCK;
		ulock.l_start = 0;
		ulock.l_whence = SEEK_SET;
		ulock.l_len = 0;

		// acquire file lock
		fcntl(fd, F_SETLKW, &alock);

		hdfsFile outfile;
		if(hdfsExists(hadoopfs, keypath.c_str()))
		{
			outfile = hdfsOpenFile(hadoopfs, keypath.c_str(), O_WRONLY, 0, 0, 0);
		}
		else
		{
			outfile = hdfsOpenFile(hadoopfs, keypath.c_str(), O_WRONLY|O_APPEND, 0, 0, 0);
		}

		if(outfile == NULL)
		{
			cout<<"[mapreduce]Debugging: openoutfile error"<<endl;
		}

		writeoutfile(&outfile, rst);
		hdfsCloseFile(hadoopfs, outfile);

		// relase file lock
		fcntl(fd, F_SETLK, &ulock);
		close(fd);
	}
	else
	{
		cout<<"[mapreduce]Warning: the write_keyvalue() function is being used outside the map function"<<endl;
	}
}

string get_nextrecord() // a user function for the map
{
	if(inside_map)
	{
		string ret = nextrecord;
		if(hdfs_getline(&input, &nextrecord))
			is_nextrec = true;
		else
			is_nextrec = false;

		return ret;
	}
	else
	{
		cout<<"[mapreduce]Warning: the get_nextrecord() function is being used outside the map function"<<endl;
		return "";
	}
}

bool get_nextinput() // internal function to process next input file
{
	if(inputpaths.size() == 0) // no more input
	{
		hdfsCloseFile(hadoopfs, input);
		return false;
	}
	else
	{
		// open another input file
		string apath = HDMR_PATH;
		apath.append(inputpaths.back());
		hdfsCloseFile(hadoopfs, input);
		input = hdfsOpenFile(hadoopfs, apath.c_str(), O_RDONLY, 0, 0, 0);
		inputpaths.pop_back();

		// pre-process first record
		if(hdfs_getline(&input, &nextrecord))
			is_nextrec = true;
		else
			is_nextrec = false;

		return true;
	}
}

bool get_nextkey(string* key) // internal function for the reduce
{
	if(inputpaths.size() == 0) // no more key
	{
		hdfsCloseFile(hadoopfs, input);
		return false;
	}
	else
	{
		*key = inputpaths.back(); // in reduce function, inputpath name is the key
		string apath = HDMR_PATH;
		apath.append(jobdirpath);
		apath.append(inputpaths.back());
		hdfsCloseFile(hadoopfs, input);
		input = hdfsOpenFile(hadoopfs, apath.c_str(), O_RDONLY, 0, 0, 0);
		inputpaths.pop_back();

		// pre-process first value 
		if(hdfs_getline(&input, &nextvalue))
			is_nextval = true;
		else
			is_nextval = false;

		stringstream ss;
		ss<<nextvalue;
		ss>>nextvalue; // key. pass this key
		ss>>nextvalue; // first value

		return true;
	}
}

bool is_nextvalue() // returns true if there is next value
{
	// check if this function is called inside the reduce function
	if(inside_reduce)
	{
		return is_nextval;
	}
	else
	{
		cout<<"[mapreduce]Warning: the is_nextvalue() function is being used outside the reduce function"<<endl;
	}
}

bool is_nextrecord()
{
	if(inside_map)
	{
		return is_nextrec;
	}
	else
	{
		cout<<"[mapreduce]Warning: the is_nextrecord() function is being used outside the map function"<<endl;
	}
}

string get_nextvalue() // returns values in reduce function
{
	// check if this function is called inside the reduce function
	if(inside_reduce)
	{
		string ret = nextvalue;

		if(hdfs_getline(&input, &nextvalue))
			is_nextval = true;
		else
			is_nextval = false;

		stringstream ss;
		ss<<nextvalue;
		ss>>nextvalue; // key. pass this key
		ss>>nextvalue; // next value

		return ret;
	}
	else
	{
		cout<<"[mapreduce]Warning: the get_nextvalue() function is being used outside the reduce function"<<endl;
		return "";
	}
}

void write_output(string record) // this user function can be used anywhere but after initialization
{
	string outpath = HDMR_PATH;
	string outpath2 = MR_PATH;
	if(outputpath == "default_output")
	{
		stringstream ss;
		ss<<"job_";
		ss<<jobid;
		ss<<".out";
		outpath.append(ss.str());
		outpath2.append(jobdirpath);
		outpath2.append(ss.str());
	}
	else
	{
		outpath.append(outputpath);
		outpath2.append(jobdirpath);
		outpath2.append(outputpath);
	}

	int fd = open(outpath2.c_str(), O_APPEND|O_SYNC|O_WRONLY|O_CREAT, 0644);
	struct flock alock;
	struct flock ulock;

	// set lock
	alock.l_type = F_WRLCK;
	alock.l_start = 0;
	alock.l_whence = SEEK_SET;
	alock.l_len = 0;

	//set unlock
	ulock.l_type = F_UNLCK;
	ulock.l_start = 0;
	ulock.l_whence = SEEK_SET;
	ulock.l_len = 0;

	// acquire file lock
	fcntl(fd, F_SETLKW, &alock);

	hdfsFile outfile;
	if(hdfsExists(hadoopfs, outpath.c_str()))
	{
		outfile = hdfsOpenFile(hadoopfs, outpath.c_str(), O_WRONLY, 0, 0, 0);
	}
	else
	{
		outfile = hdfsOpenFile(hadoopfs, outpath.c_str(), O_WRONLY|O_APPEND, 0, 0, 0);
	}

	if(outfile == NULL)
	{
		cout<<"[mapreduce]Debugging: openoutfile error"<<endl;
	}

	writeoutfile(&outfile, record);
	hdfsCloseFile(hadoopfs, outfile);

	// relase file lock
	fcntl(fd, F_SETLK, &ulock);
	close(fd);
}

int get_jobid()
{
	return jobid;
}

int writeoutfile(hdfsFile* file, string data)
{
	data.append("\n");
	memset(write_buf, 0, BUF_SIZE);
	strcpy(write_buf, data.c_str());
	hdfsWrite(hadoopfs, *file, write_buf, strlen(write_buf));
}

bool hdfs_getline(hdfsFile* file, string* ret)
{
	int pos = 0;
	int bufsize = 0;
	bufsize = hdfsRead(hadoopfs, *file, read_buf, BUF_SIZE);
	if(bufsize == 0)
	{
		return false;
	}
	else
	{
		*ret = read_buf;
		pos = ret->find_first_of('\n');
		if(pos == string::npos)
		{
			// TODO: append next buffer
		}
		*ret = ret->substr(0, pos);
		hdfsSeek(hadoopfs, *file, hdfsTell(hadoopfs, *file)-(bufsize-pos-1));

		return true;
	}
}

#endif
