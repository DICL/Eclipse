#ifndef _CONNSLAVE_
#define _CONNSLAVE_

#include <iostream>
#include <mapreduce/definitions.hh>

class connslave // connection to the slave
{
private:
	int numslot;
	int numrunningslot;
	int fd;
public:
	connslave(int fd);
	connslave(int numslot, int fd);
	~connslave();
	int getnumslot();
	int getnumrunningslot();
	void setnumslot(int num);
	void setnumrunningslot(int num);
	int getfd();
};

connslave::connslave(int fd)
{
	this->numslot = 0;
	this->numrunningslot = 0;
	this->fd = fd;
}

connslave::connslave(int numslot, int fd)
{
	this->numslot = numslot;
	this->numrunningslot = 0;
	this->fd = fd;
}
connslave::~connslave()
{
	close(fd);
}

int connslave::getfd()
{
	return this->fd;
}

int connslave::getnumslot()
{
	return this->numslot;
}

int connslave::getnumrunningslot()
{
	return this->numrunningslot;
}

void connslave::setnumslot(int num)
{
	this->numslot = num;
}

void connslave::setnumrunningslot(int num)
{
	this->numrunningslot = num;
}

#endif
