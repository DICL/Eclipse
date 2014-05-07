#include <iostream>
#include <pthread.h>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <mapreduce/definitions.hh>
#include "slave.hh"
#include "../slave_job.hh"
#include "../slave_task.hh"

using namespace std;

char read_buf[BUF_SIZE];
char write_buf[BUF_SIZE];

int port = -1;
int dhtport = -1;
int masterfd = -1;
bool master_is_set = false;
char master_address[BUF_SIZE];
string localhostname;
vector<slave_job*> running_jobs; // a vector of job, one or multiple tasks of which are running on this slave node
vector<slave_task*> running_tasks; // a vector of running tasks

int main(int argc, char** argv)
{
	// initialize data structures from setup.conf
	ifstream conf;
	string token;
	string confpath = LIB_PATH;
	confpath.append("setup.conf");
	conf.open(confpath.c_str());

	conf>>token;
	while(!conf.eof())
	{
		if(token == "dhtport")
		{
			conf>>token;
			dhtport = atoi(token.c_str());
		}
		else if(token == "port")
		{
			conf>>token;
			port = atoi(token.c_str());
		}
		else if(token == "max_job")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "num_slave")
		{
			// ignore and just pass through this case
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
			cout<<"[slave]Unknown configure record: "<<token<<endl;
		}
		conf>>token;
	}
	conf.close();

	// verify initialization
	if(port == -1)
	{
		cout<<"[slave]port should be specified in the setup.conf"<<endl;
		return 1;
	}
	if(master_is_set == false)
	{
		cout<<"[slave]master_address should be specified in the setup.conf"<<endl;
		return 1;
	}

	// read hostname from hostname file
	ifstream hostfile;
	string hostpath = DHT_PATH;
	hostpath.append("hostname");
	hostfile.open(hostpath.c_str());
	hostfile>>localhostname;

	// connect to master
	masterfd = connect_to_server(master_address, port);
	if(masterfd<0)
	{
		cout<<"[slave]Connecting to master failed"<<endl;
		return 1;
	}

	// set master socket to be non-blocking socket to avoid deadlock
	fcntl(masterfd, F_SETFL, O_NONBLOCK);


	signal_listener();

	return 0;
}

int connect_to_server(char* host, unsigned short port)
{
	int clientfd;
	struct sockaddr_in serveraddr;
	struct hostent* hp;

	//SOCK_STREAM -> tcp
	clientfd = socket(AF_INET, SOCK_STREAM, 0);
	if(clientfd < 0)
	{
		cout<<"[slave]Openning socket failed"<<endl;
		exit(1);
	}

	hp = gethostbyname(host);

	if (hp == NULL)
		cout<<"[slave]Cannot find host by host name"<<endl;

	memset((void*) &serveraddr, 0, sizeof(struct sockaddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
	return clientfd;
}

void signal_listener()
{
	//ofstream logfile = new ofstream("slave" + id + ".log");
	// get signal from master, jobs and tasks
	int readbytes = 0;
	int writeidclock = 0;
	struct timeval time_start;
	struct timeval time_end;

	gettimeofday(&time_start, NULL);
	gettimeofday(&time_end, NULL);

	while(1)
	{
		readbytes = nbread(masterfd, read_buf);
		if(readbytes == 0) //connection closed from master
		{
//logfile << gettimeofday
			cout<<"[slave]Connection from master is abnormally closed"<<endl;
			while(close(masterfd)<0)
			{
				cout<<"[slave]Closing socket failed"<<endl;

				// sleeps for 1 milli second
				usleep(1000);
			}
			cout<<"[slave]Exiting slave..."<<endl;
			exit(0);
		}
		else if(readbytes < 0)
		{
			// do nothing
		}
		else // signal arrived from master
		{
			if(strncmp(read_buf, "whoareyou", 9) == 0)
			{
				memset(write_buf, 0, BUF_SIZE);
				strcpy(write_buf, "slave");
				nbwrite(masterfd, write_buf);
			}
			else if(strncmp(read_buf, "close", 5) == 0)
			{
				cout<<"[slave]Close request from master"<<endl;
				while(close(masterfd)<0)
				{
					cout<<"[slave]Close failed"<<endl;

					// sleeps for 1 milli seconds
					usleep(1000);
				}
				cout<<"[slave]Exiting slave..."<<endl;
				exit(0);
			}
			else if(strncmp(read_buf, "tasksubmit", 10) == 0)
			{
				// launch the forwarded task
				slave_job* thejob = NULL;
				slave_task* thetask = NULL;
				char* token;
				token = strtok(read_buf, " "); // token -> tasksubmit
				token = strtok(NULL, " "); // token -> jobid expected

				// parse task configure
				while(token != NULL)
				{
					if(strncmp(token, "jobid", 5) == 0)
					{
						int id;
						token = strtok(NULL, " ");
						id = atoi(token);

						thejob = find_jobfromid(id);

						if(thejob == NULL) // if task in this job already running in this slave
						{
							thejob = new slave_job(id);
							running_jobs.push_back(thejob);
						}
					}
					else if(strncmp(token, "taskid", 6) == 0)
					{
						int id;
						if(thejob == NULL)
						{
							cout<<"Debugging: the job is set to null in";
							cout<<"the slave side when a task is forwarded."<<endl;
							token = strtok(NULL, " ");
							continue;
						}
						token = strtok(NULL, " ");
						id = atoi(token);
						thetask = new slave_task(id); // the status is running by default

						// add to the running_tasks vector
						running_tasks.push_back(thetask);

						// add this task in 'thejob'
						thejob->add_task(thetask);
					}
					else if(strncmp(token, "role", 4) == 0)
					{
						if(thetask == NULL)
						{
							cout<<"Debugging: the task is set to null in ";
							cout<<"the slave side when a task is forwarded."<<endl;
							token = strtok(NULL, " ");
							continue;
						}
						token = strtok(NULL, " "); // "MAP" or "REDUCE"
						if(strncmp(token, "MAP", 3) == 0)
						{
							thetask->set_taskrole(MAP);
						}
						else if(strncmp(token, "REDUCE", 6) == 0)
						{
							thetask->set_taskrole(REDUCE);
						}
						else
						{
							cout<<"Debugging: the task role is undefined well."<<endl;
							thetask->set_taskrole(JOB);
						}
					}
					else if(strncmp(token, "inputpath", 9) == 0)
					{
						string tmp; // used for temporarily storing the paths
						int numpaths;
						token = strtok(NULL, " "); // token -> number of input paths
						numpaths = atoi(token);
						for(int i=0; i<numpaths; i++)
						{
							token = strtok(NULL, " ");
							tmp = token;
							thetask->add_inputpath(tmp);
						}
					}
					else if(strncmp(token, "argcount", 8) == 0)
					{
						// deal with argcount and argvalues together in this bracket
						// because both configure should be successive
						int count;
						token = strtok(NULL, " "); // token -> arguments count
						count = atoi(token);
						thetask->set_argcount(count);

						char** values = new char*[count];

						token = strtok(NULL, " "); // token -> argvalues

						// check the message protocol
						if(strncmp(token, "argvalues", 9) != 0)
						{
							cout<<"Debugging: the 'tasksubmit' protocol conflicts in the slave."<<endl;
							// process next configure
							token = strtok(NULL, " ");
							continue;
						}

						// parse all arguments
						for(int i=0;i<count;i++)
						{
							token = strtok(NULL, " ");
							values[i] = new char[strlen(token)+1];
							strcpy(values[i], token);
						}
						thetask->set_argvalues(values);
					}
					else
					{
						cout<<token<<": unknown task configure in the slave side"<<endl;
					}

					// process next configure
					token = strtok(NULL, " ");
				}

				// launch the forwarded task
				launch_task(thetask, writeidclock++);
			}
			else
			{
				cout<<"[slave]Undefined signal from master: "<<read_buf<<endl;
				cout<<"[slave]Undefined signal size: "<<readbytes<<endl;
			}
		}

		// check the running_jobs
		for(int i=0;(unsigned)i<running_jobs.size();i++)
		{
			// send key information of the job to the master node
			while(running_jobs[i]->is_unreportedkey())
			{
				stringstream ss;
				string key = running_jobs[i]->pop_unreportedkey();
				string keystr = "key";

				ss<<" jobid ";
				ss<<running_jobs[i]->get_jobid();
				keystr.append(ss.str());

				keystr.append(" ");
				keystr.append(key);
				memset(write_buf, 0, BUF_SIZE);
				strcpy(write_buf, keystr.c_str());

				// send message to the master node
				nbwrite(masterfd, write_buf);
			}

			// check if all tasks in the job are finished
			if(running_jobs[i]->get_numrunningtasks() == 0) // all task is finished
			{
				// clear job from the vectors
				slave_job* deleted_job = running_jobs[i];
				running_jobs.erase(running_jobs.begin()+i);
				i--;
				delete deleted_job;
				continue;
			}
		}

		// check message from tasks through pipe
		for(int i=0;(unsigned)i<running_tasks.size();i++)
		{
			readbytes = nbread(running_tasks[i]->get_readfd(), read_buf);
			if(readbytes == 0)
			{
				// ignore this case as default
			}
			else if(readbytes < 0)
			{
				continue;
			}
			else
			{
				if(strncmp(read_buf, "complete", 8) == 0)
				{
					//cout<<"[slave]Task with taskid "<<running_tasks[i]->get_taskid();
					//cout<<" and job id "<<running_tasks[i]->get_job()->get_jobid();
					//cout<<" completed successfully"<<endl;

					// send terminate message
					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, "terminate");
					nbwrite(running_tasks[i]->get_writefd(), write_buf);

					// mark the task as completed
					running_tasks[i]->set_status(COMPLETED);
				}
				else if(strncmp(read_buf, "requestconf", 11) == 0)
				{
					// parse all task configure
					stringstream message;
					message<<"taskconf ";

					// job id
					message<<"jobid ";
					message<<running_tasks[i]->get_job()->get_jobid();

					// task id
					message<<" taskid ";
					message<<running_tasks[i]->get_taskid();

					// input paths
					message<<" inputpaths ";
					message<<running_tasks[i]->get_numinputpaths(); // number of inputpaths
					for(int j=0;j<running_tasks[i]->get_numinputpaths();j++)
					{
						message<<" ";
						message<<running_tasks[i]->get_inputpath(j);
					}
					
					// send message to the task
					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, message.str().c_str());
					nbwrite(running_tasks[i]->get_writefd(), write_buf);
				}
				else if(strncmp(read_buf, "key", 3) == 0)
				{
					char* token;
					token = strtok(read_buf, " "); // token <- key
					token = strtok(NULL, " "); // token <- key value
					running_tasks[i]->get_job()->add_key(token);
				}
				else
				{
					cout<<"[slave]Undefined message protocol from task"<<endl;
					cout<<"       Message: "<<read_buf<<endl;
				}
			}
		}

		// check task clear
		for(int i=0;(unsigned)i<running_tasks.size();i++)
		{
			if(waitpid(running_tasks[i]->get_pid(), &(running_tasks[i]->pstat), WNOHANG)) // waitpid returned nonzero 
			{
				if(running_tasks[i]->get_status() == COMPLETED) // successful termination
				{
					// send 'taskcomplete' message to the master
					stringstream ss;
					string msg = "taskcomplete";
					ss<<" jobid ";
					ss<<running_tasks[i]->get_job()->get_jobid();
					ss<<" taskid ";
					ss<<running_tasks[i]->get_taskid();
					msg.append(ss.str());

					memset(write_buf, 0, BUF_SIZE);
					strcpy(write_buf, msg.c_str());
					nbwrite(masterfd, write_buf);

					// clear all to things related to this task
					running_tasks[i]->get_job()->finish_task(running_tasks[i]);
					delete running_tasks[i];
					running_tasks.erase(running_tasks.begin()+i);
					i--;
				}
				else
				{
					cout<<"[slave]A ";
					if(running_tasks[i]->get_taskrole() == MAP)
						cout<<"map ";
					else if(running_tasks[i]->get_taskrole() == REDUCE)
						cout<<"reduce ";
					cout<<"task with taskid "<<running_tasks[i]->get_taskid();
					cout<<" and jobid "<<running_tasks[i]->get_job()->get_jobid();
					cout<<" terminated abnormally"<<endl;
					cout<<"pid: "<<running_tasks[i]->get_pid()<<endl;
					// TODO: clear data structures for the task

					// TODO: launch the failed task again
				}
			}
		}

		gettimeofday(&time_end, NULL);
		if(time_end.tv_sec - time_start.tv_sec > 20.0)
		{
			//cout<<"[Slave Heartbeat]";
			//cout<<"numjob: "<<running_jobs.size()<<", ";
			//cout<<"numtask: "<<running_tasks.size()<<"("<<localhostname<<")"<<endl;
			gettimeofday(&time_start, NULL);
		}
	}
	while(close(masterfd)<0)
	{
		cout<<"[slave]Close failed"<<endl;
		
		// sleeps for 1 milliseconds
		usleep(1000);
	}
	cout<<"[slave]Exiting slave..."<<endl;
	exit(0);
}

void launch_task(slave_task* atask, int writeid)
{
	int pid;
	int fd1[2]; // two set of fds between slave and task(1)
	int fd2[2]; // two set of fds between slave and task(2)
	pipe(fd1); // fd1[0]: slave read, fd1[1]: task write
	pipe(fd2); // fd2[0]: task read, fd2[1]: slave write

	// set pipe fds to be non-blocking to avoid deadlock
	fcntl(fd1[0], F_SETFL, O_NONBLOCK);
	fcntl(fd1[1], F_SETFL, O_NONBLOCK);
	fcntl(fd2[0], F_SETFL, O_NONBLOCK);
	fcntl(fd2[1], F_SETFL, O_NONBLOCK);

	// set pipe fds
	atask->set_readfd(fd1[0]);
	atask->set_writefd(fd2[1]);

	pid = fork();

	if(pid == 0) // child side
	{
		// pass all arguments
		char** args;
		int count;
		stringstream ss;
		stringstream ss1;
		stringstream ss2;

		// origianl arguments + write id + pipe fds + task type
		count = atask->get_argcount();
		args = new char*[count+4];

		// pass original arguments
		for(int i=0;i<count;i++)
		{
			args[i] = new char[strlen(atask->get_argvalues()[i])+1];
			strcpy(args[i], atask->get_argvalues()[i]);
			args[i][strlen(atask->get_argvalues()[i])] = 0;
		}

		// pass write id
		ss << writeid;

		args[count] = new char[ss.str().length()+1];
		strcpy(args[count], ss.str().c_str());
		args[count][ss.str().length()] = 0;

		// pass pipe fds
		ss1<<fd2[0];
		ss2<<fd1[1];

		args[count+1] = new char[ss1.str().length()+1];
		args[count+2] = new char[ss2.str().length()+1];
		strcpy(args[count+1], ss1.str().c_str());
		strcpy(args[count+2], ss2.str().c_str());
		args[count+1][ss1.str().length()] = 0;
		args[count+2][ss2.str().length()] = 0;

		// pass task type
		if(atask->get_taskrole() == MAP)
		{
			args[count+3] = new char[4];
			strcpy(args[count+3], "MAP");
			args[count+3][3] = 0;
			//args[count+2] = "MAP";
		}
		else if(atask->get_taskrole() == REDUCE)
		{
			args[count+3] = new char[7];
			strcpy(args[count+3], "REDUCE");
			args[count+3][6] = 0;
			//args[count+2] = "REDUCE";
		}
		else
		{
			cout<<"[slave]Debugging: the role of the task is not defined in launch_task() function"<<endl;
			args[count+3] = new char[4];
			strcpy(args[count+3], "JOB");
			//args[count+2] = "JOB";
		}
		// pass null to last parameter
		args[count+4] = NULL;

		// launch the task with the passed arguments
		while(execv(args[0], args) == -1)
		{
			cout<<"Debugging: execv failed"<<endl;
			cout<<"Arguments:";
			for(int i=0;i<count+3;i++)
			{
				cout<<" "<<args[i];
			}
			cout<<endl;

			// sleeps for 1 seconds and retry execv. change this if necessary
			sleep(1);
		}
	}
	else if(pid < 0)
	{
		cout<<"[slave]Task could not have been started due to child process forking failure"<<endl;
	}
	else // parent side
	{
		// close pipe fds for task side
		close(fd2[0]);
		close(fd1[1]);

		// register the pid of the task process
		atask->set_pid(pid);

		// print the launch message
		//cout<<"[slave]A ";
		//if(atask->get_taskrole() == MAP)
		//	cout<<"map ";
		//else if(atask->get_taskrole() == REDUCE)
		//	cout<<"reduce ";
		//cout<<"task launched with taskid "<<atask->get_taskid()<<" and jobid "<<atask->get_job()->get_jobid();
		//cout<<endl;
		return;
	}
}

slave_job* find_jobfromid(int id)
{
	for(int i=0;(unsigned)i<running_jobs.size();i++)
	{
		if(running_jobs[i]->get_jobid() == id)
		{
			return running_jobs[i];
		}
	}
	return NULL;
}
