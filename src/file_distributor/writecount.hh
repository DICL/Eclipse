#ifndef __WRITECOUNT__
#define __WRITECOUNT__

#include <iostream>

using namespace std;

class writecount
{
	private:
		vector<int> peerids;
		int writeid;

	public:
		writecount(int anid);
		int increment();
		int decrement();
		int get_count();
		int get_id();
		void set_count(int num);
		void set_id(int num);

};

writecount::writecount(int anid)
{
	writeid = anid;
}

int writecount::get_count()
{
	return count;
}

void writecount::set_count(int num)
{
	count = num;
}

int writecount::get_id()
{
	return writeid;
}

void writecount::set_id(int num)
{
	writeid = num;
}

class peerwritecount
{
	priavet:
		int peerid;
		int writeid;
		int count;
	public:
};

#endif
