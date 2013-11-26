#ifndef _MASTER_JOB_
#define _MASTER_JOB_

#include <iostream>
#include <string>
#include <master/dec_connclient.hh>

using namespace std;

class master_job
{
private:
	int jobpid;
	int readfd;
	int writefd;
	int nummap;
	int numreduce;
	int pipefds[2]; // pipe file descriptors to close for destructor
	string programname;
	connclient* client;
public:
	master_job();
	master_job(connclient* aclient);
	master_job(int pid);
	master_job(int pid, int read, int write);
	~master_job();
	void clear();
	int getreadfd();
	void setreadfd(int num);
	int getwritefd();
	void setwritefd(int num);
	void setnummap(int num);
	int getnummap();
	void setnumreduce(int num);
	int getnumreduce();
	int getjobpid();
	void setjobpid(int num);
	connclient* getclient();
	void setclient(connclient* aclient);
	void setpipefds(int num1, int num2);
	string getprogramname();
	void setprogramname(string path);
};

master_job::master_job()
{
	this->nummap = -1;
	this->numreduce = -1;
	this->jobpid = -1;
	this->readfd = -1;
	this->writefd = -1;
	this->pipefds[0] = -1;
	this->pipefds[1] = -1;
	this->client = NULL;
}

master_job::master_job(connclient* aclient)
{
	this->nummap = -1;
	this->numreduce = -1;
	this->jobpid = -1;
	this->readfd = -1;
	this->writefd = -1;
	this->pipefds[0] = -1;
	this->pipefds[1] = -1;
	this->client = aclient;
}

master_job::master_job(int pid)
{
	this->nummap = -1;
	this->numreduce = -1;
	this->jobpid = pid;
	this->readfd = -1;
	this->writefd = -1;
	this->pipefds[0] = -1;
	this->pipefds[1] = -1;
	this->client = NULL;
}

master_job::master_job(int pid, int read, int write)
{
	this->nummap = -1;
	this->numreduce = -1;
	this->jobpid = pid;
	this->readfd = read;
	this->writefd = write;
	this->pipefds[0] = -1;
	this->pipefds[1] = -1;
	this->client = NULL;
}

master_job::~master_job()
{
	this->client->setrunningjob(NULL);
	close(readfd);
	close(writefd);
	close(pipefds[0]);
	close(pipefds[1]);
}

void master_job::clear()
{
	close(readfd);
	close(writefd);
}

int master_job::getreadfd()
{
	return this->readfd;
}

int master_job::getwritefd()
{
	return this->writefd;
}

void master_job::setreadfd(int num)
{
	this->readfd = num; 
}

void master_job::setwritefd(int num)
{
	this->writefd = num; 
}

int master_job::getjobpid()
{
	return this->jobpid;
}

void master_job::setjobpid(int num)
{
	this->jobpid = num;
}

connclient* master_job::getclient()
{
	return this->client;
}
	
void master_job::setclient(connclient* aclient)
{
	this->client = aclient;
}

void master_job::setpipefds(int num1, int num2)
{
	this->pipefds[0] = num1;
	this->pipefds[1] = num2;
}

string master_job::getprogramname()
{
	return this->programname;
}

void master_job::setprogramname(string name)
{
	this->programname = name;
}

void master_job::setnummap(int num)
{
	this->nummap = num;
}

int master_job::getnummap()
{
	return this->nummap;
}

void master_job::setnumreduce(int num)
{
	this->numreduce = num;
}

int master_job::getnumreduce()
{
	return this->numreduce;
}

#endif
