#ifndef __FILE_CONNCLIENT__
#define __FILE_CONNCLIENT__

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include "messagebuffer.hh"
#include "writecount.hh"
#include <mapreduce/definitions.hh>

using namespace std;

class file_connclient
{
private:
	int fd;
	//file_role role;

public:
	writecount* thecount;
	vector<messagebuffer*> msgbuf;

	file_connclient(int fd);
	//file_connclient(int fd, file_role arole);
	~file_connclient();

	int get_fd();
	//int get_writeid();
	void set_fd(int num);
	//void set_writeid(int num);
	//void set_role(file_role arole);
	//file_role get_role();
};

file_connclient::file_connclient(int fd)
{
	this->fd = fd;
	thecount = NULL;
	//this->writeid = -1;
	//this->role = UNDEFINED;

	// add a null buffer
	msgbuf.push_back(new messagebuffer());
}

//file_connclient::file_connclient(int fd, file_role arole)
//{
//	this->fd = fd;
//	//this->role = arole;
//	this->writeid = -1;
//
//	// add a null buffer
//	msgbuf.push_back(new messagebuffer());
//}

file_connclient::~file_connclient()
{
	for(int i = 0; (unsigned)i < msgbuf.size(); i++)
	{
		delete msgbuf[i];
	}

	if(thecount != NULL)
		delete thecount;
}

int file_connclient::get_fd()
{
	return this->fd;
}

void file_connclient::set_fd(int num)
{
	this->fd = num;
}

//void file_connclient::set_role(file_role arole)
//{
//	this->role = arole;
//}

//file_role file_connclient::get_role()
//{
//	return this->role;
//}

//int file_connclient::get_writeid()
//{
//	return writeid;
//}

//void file_connclient::set_writeid(int num)
//{
//	writeid = num;
//}

#endif
