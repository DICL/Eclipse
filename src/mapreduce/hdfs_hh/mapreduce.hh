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
void set_mapper(void (*map_func) (string record));
void set_reducer(void (*red_func) (string key));
bool is_nextvalue(); // return true if there is next value
string get_nextvalue(); // returns values in reduce function
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
string hdfs_getline(hdfsFile* file);

mr_role role = JOB;
char read_buf[BUF_SIZE]; // read buffer for pipe
char write_buf[BUF_SIZE]; // write buffer for pipe

int argcount = -1;
char** argvalues = NULL;

// variables for job role
int port = -1;
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
void (*mapfunction) (string record); // map function pointer
set<string> reported_keys;
set<string> unreported_keys;

// variables for reduce task
void (*reducefunction) (string key); // reduce function pointer
string nextvalue;
bool is_nextval = false;

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
			if(token == "backlog")
			{
				// ignore and just pass throught this case
				conf>>token;
			}
			else if(token == "port")
			{
				conf>>token;
				port = atoi(token.c_str());
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
		readbytes = read(masterfd, read_buf, BUF_SIZE);

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
				write(masterfd, "job", BUF_SIZE);

				// blocking read of job id
				while(1)
				{
					memset(read_buf, 0, BUF_SIZE);
					readbytes = read(masterfd, read_buf, BUF_SIZE);
					if(readbytes == 0)
					{
						cout<<"[mapreduce]Connection from master abnormally close"<<endl;
						break;
					}
					else if(readbytes < 0)
					{
						// sleep for 0.0001 second. change this if necessary
						usleep(100);
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
			}
		}

		// set master fd to be nonblocking to avoid deadlock
		fcntl(masterfd, F_SETFL, O_NONBLOCK);

		// make directories for the job
		string apath = HDMR_PATH;
		stringstream jobidss;

		// pass job id to the string stream
		jobidss<<".job_";
		jobidss<<jobid;
		jobidss<<"/";
		jobdirpath = jobidss.str();
		apath.append(jobdirpath);

		hdfsCreateDirectory(hadoopfs, apath);
	}
	else // when the role is map task or reduce task
	{
		pipefd[0] = atoi(argv[argc-3]); // read fd
		pipefd[1] = atoi(argv[argc-2]); // write fd
		argcount = argc - 3;

		// request the task configuration
		write(pipefd[1], "requestconf", BUF_SIZE);

		// blocking read until the arrival of 'taskconf' message from master
		while(1)
		{
			memset(read_buf, 0, BUF_SIZE);
			readbytes = read(pipefd[0], read_buf, BUF_SIZE);
			if(readbytes == 0)
			{
				cout<<"[mapreduce]the connection from slave node is abnormally closed"<<endl;
			}
			else if(readbytes < 0)
			{
				// sleep for 0.0001 second. change this if necessary
				usleep(100);
			}
			else
			{
				break;
			}
		}
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
			string apath = HDMR_PATH;
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
			strcpy(write_buf, write_string.c_str());
			write(masterfd, write_buf, BUF_SIZE);
		}

		// blocking read from master until "complete" receiving message
		while(1)
		{
			readbytes = read(masterfd, read_buf, BUF_SIZE);
			if(readbytes == 0) // master abnormally terminated
			{
				// TODO: Terminate the job properly
				exit(0);
			}
			else if(readbytes < 0)
			{
				// sleeps for 0.0001 seconds. change this if necessary
				usleep(100);
				continue;
			}
			else
			{
				if(strncmp(read_buf, "complete", 9) == 0) // "complete" message received
				{
					cout<<"[mapreduce]Job is successfully completed"<<endl;
					break;
				}
				else if(strncmp(read_buf, "mapcomplete", 11) == 0)
				{
					cout<<"[mapreduce]Map tasks are completed"<<endl;
					cout<<"[mapreduce]Now reduce tasks are launched"<<endl;
				}
				else // all other messages are ignored
					continue;
			}
		}

		// remove the job directory
		string apath = HDMR_PATH;
		apath.append(jobdirpath);
		hdfsDelete(hadoopfs, apath.c_str());

		// disonnect from hdfs server and close the hdfs file
		hdfsCloseFile(hadoopfs, input);
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
			while(get_record(&record))
			{
				inside_map = true;
				(*mapfunction)(record);
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
					strcpy(write_buf, keystr.c_str());
					while(write(pipefd[1], write_buf, BUF_SIZE)< 0)
					{
						cout<<"[mapreduce]write to slave failed"<<endl;
						int err = errno;
						if(err == EAGAIN)
						{
							cout<<"[mapreduce]due to the EAGAIN"<<endl;
						}
						else if(err ==  EFAULT)
						{
							cout<<"[mapreduce]due to the EFAULT"<<endl;
						}
						// sleeps for 1 second. change this if necessary
						sleep(1);
					}

					// sleeps for 0.0001 seconds. change this if necessary
					usleep(100);
				}
			}
		}

		// send complete message
		write(pipefd[1], "complete", BUF_SIZE);

		// blocking read until the 'terminate' message
		while(1)
		{
			readbytes = read(pipefd[0], read_buf, BUF_SIZE);
			if(readbytes == 0) // pipe fd was closed abnormally
			{
				// TODO: Terminate the task properly
				hdfsCloseFile(hadoopfs, input);
				hdfsDisconnect(hadoopfs);
				exit(0);
			}
			else if(readbytes > 0)
			{
				if(strncmp(read_buf, "terminate", 9) == 0)
				{
					//cout<<"[mapreduce]Map task is successfully completed"<<endl;

					// clear task
					hdfsCloseFile(hadoopfs, input);
					hdfsDisconnect(hadoopfs);

					// terminate successfully
					exit(0);
				}
				else // all other messages are ignored
					continue;
			}

			// sleeps for 0.0001 seconds. change this if necessary
			usleep(100);
		}
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
		write(pipefd[1], "complete", BUF_SIZE);

		// blocking read until 'terminate' message arrive
		while(1)
		{
			readbytes = read(pipefd[0], read_buf, BUF_SIZE);
			if(readbytes == 0) // pipe fd was closed abnormally
			{
				// TODO: Terminate the task properly
				hdfsCloseFile(hadoopfs, input);
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
					hdfsCloseFile(hadoopfs, input);
					hdfsDisconnect(hadoopfs);
					// terminate successfully
					exit(0);
				}
				else // all other messages are ignored
					continue;
			}

			// sleeps for 0.0001 seconds. change this if necessary
			usleep(100);
		}
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
void set_mapper(void (*map_func) (string record))
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
			strcpy(write_buf, keystr.c_str());
			while(write(pipefd[1], write_buf, BUF_SIZE)< 0)
			{
				cout<<"[mapreduce]write to slave failed"<<endl;
				int err = errno;
				if(err == EAGAIN)
				{
					cout<<"[mapreduce]due to the EAGAIN"<<endl;
				}
				else if(err == EFAULT)
				{
					cout<<"[mapreduce]due to the EFAULT"<<endl;
				}
				// sleeps for 1 second. change this if necessary
				sleep(1);
			}

			// sleeps for 0.0001 seconds. change this if necessary
			usleep(100);
		}

		// path of the key file
		string keypath = HDMR_PATH;
		keypath.append(jobdirpath);
		keypath.append(key);

		// result string
		string rst = key;
		rst.append(" ");
		rst.append(value);
		int fd = openoutfile(keypath);

		if(fd<0)
		{
			cout<<"[mapreduce]Debugging: openoutfile error"<<endl;
		}

		writeoutfile(fd, rst);
		closeoutfile(fd);
	}
	else
	{
		cout<<"[mapreduce]Warning: the write_keyvalue() function is being used outside the map function"<<endl;
	}
}

bool get_record(string* record) // internal function for the map
{
	getline(input, *record);

	while(input.eof() || !input.is_open()) // process input data until get record
	{
		if(inputpaths.size() == 0) // no more record
		{
			input.close();
			return false;
		}

		// open another input data
		string apath = HDMR_PATH;
		apath.append(inputpaths.back());
		input.close();
		input.open(apath.c_str());
		inputpaths.pop_back();

		getline(input, *record);
	}

	// got record successfully
	return true;
}

bool get_nextkey(string* key) // internal function for the reduce
{
	if(inputpaths.size() == 0) // no more key
	{
		input.close();
		return false;
	}
	else
	{
		*key = inputpaths.back(); // in reduce function, inputpath name is the key
		string apath = HDMR_PATH;
		apath.append(jobdirpath);
		apath.append(inputpaths.back());
		input.close();
		input.open(apath.c_str());
		inputpaths.pop_back();

		// pre-process first value 
		input>>nextvalue; // key. pass this key
		input>>nextvalue; // first value

		if(input.eof())
			is_nextval = false;
		else
			is_nextval = true;

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

string get_nextvalue() // returns values in reduce function
{
	// check if this function is called inside the reduce function
	if(inside_reduce)
	{
		string ret = nextvalue;
		input>>nextvalue; // key. pass this key
		input>>nextvalue; // next value

		if(input.eof())
			is_nextval = false;
		else
			is_nextval = true;

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
	hdfsFile outfile;
	string outpath = HDMR_PATH;
	if(outputpath == "default_output")
	{
		stringstream ss;
		ss<<"job_";
		ss<<jobid;
		ss<<".out";
		outpath.append(ss.str());
	}
	else
	{
		outpath.append(outputpath);
	}
	if(hdfsExists(hadoopfs, outpath))
	{
		outfile = hdfsOpenFile(hadoopfs, outpath, O_WRONLY, 0, 0, 0);
	}
	else
	{
		outfile = hdfsOpenFile(hadoopfs, outpath, O_WRONLY|O_APPEND, 0, 0, 0);
	}

	record.append("\n");
	strcpy(write_buf, record.c_str());

	hdfsWrite(hadoopfs, outfile, write_buf, strlen(write_buf));
	hdfsCloseFile(hadoopfs, outfile);
}

int get_jobid()
{
	return jobid;
}

int writeoutfile(hdfsFile* hdfsFile, string data)
{
	data.append("\n");
	strcpy(write_buf, data.c_str());
	hdfsWrite(hadoopfs, write_buf, strlen(write_buf.c_str()));
}

string hdfs_getline(hdfsFile* file)
{
	return "";
}

#endif
