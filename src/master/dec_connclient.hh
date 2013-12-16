#ifndef _DEC_CONNCLIENT_
#define _DEC_CONNCLIENT_


#include <iostream>
#include "master_job.hh"

class master_job;

class connclient // connection to the client
{
private:
	int fd;
	master_job* runningjob;
public:
	connclient(int fd);
	~connclient();
	int getfd();
	void setfd(int num);
	master_job* getrunningjob();
	void setrunningjob(master_job* ajob);
	void clearjob(); // this function clears job
};

#endif
