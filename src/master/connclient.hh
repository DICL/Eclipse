#ifndef _CONNCLIENT_
#define _CONNCLIENT_

#include <iostream>

class connclient // connection to the client
{
private:
	int fd;
	char read_buf[BUF_SIZE];
	int readbytes;

public:
	connclient(int fd);
	~connclient();
	int getfd();
	void setfd(int num);
	int get_readbytes();
	void set_readbytes(int abytes);
	char* get_read_buf();

};

connclient::connclient(int fd)
{
	this->fd = fd;
	this->readbytes = 0;
}

connclient::~connclient()
{
	close(fd);
}

int connclient::getfd()
{
	return this->fd;
}

void connclient::setfd(int num)
{
	this->fd = num;
}

int connclient::get_readbytes()
{
	return this->readbytes;
}

void connclient::set_readbytes(int abytes)
{
	this->readbytes = abytes;
}

char* connclient::get_read_buf()
{
	return this->read_buf;
}

#endif
