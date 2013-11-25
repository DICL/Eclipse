#ifndef _DEC_CONNCLIENT_
#define _DEC_CONNCLIENT_


#include <iostream>
#include <mapreduce/job.hh>

class job;

class connclient // connection to the client
{
private:
	int fd;
	job* runningjob;
public:
	connclient(int fd);
	~connclient();
	int getfd();
	void setfd(int num);
	job* getrunningjob();
	void setrunningjob(job* ajob);
	void clearjob(); // this function clears job
};

#endif
