#ifndef _JOB_
#define _JOB_

#include <iostream>
#include <string>
#include <master/dec_connclient.hh>

using namespace std;

class job
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
	job();
	job(connclient* aclient);
	job(int pid);
	job(int pid, int read, int write);
	~job();
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

job::job()
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

job::job(connclient* aclient)
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

job::job(int pid)
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

job::job(int pid, int read, int write)
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

job::~job()
{
	this->client->setrunningjob(NULL);
	close(readfd);
	close(writefd);
	close(pipefds[0]);
	close(pipefds[1]);
}

void job::clear()
{
	close(readfd);
	close(writefd);
}

int job::getreadfd()
{
	return this->readfd;
}

int job::getwritefd()
{
	return this->writefd;
}

void job::setreadfd(int num)
{
	this->readfd = num; 
}

void job::setwritefd(int num)
{
	this->writefd = num; 
}

int job::getjobpid()
{
	return this->jobpid;
}

void job::setjobpid(int num)
{
	this->jobpid = num;
}

connclient* job::getclient()
{
	return this->client;
}
	
void job::setclient(connclient* aclient)
{
	this->client = aclient;
}

void job::setpipefds(int num1, int num2)
{
	this->pipefds[0] = num1;
	this->pipefds[1] = num2;
}

string job::getprogramname()
{
	return this->programname;
}

void job::setprogramname(string name)
{
	this->programname = name;
}

void job::setnummap(int num)
{
	this->nummap = num;
}

int job::getnummap()
{
	return this->nummap;
}

void job::setnumreduce(int num)
{
	this->numreduce = num;
}

int job::getnumreduce()
{
	return this->numreduce;
}

#endif
