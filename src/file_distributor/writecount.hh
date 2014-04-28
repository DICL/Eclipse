#ifndef __WRITECOUNT__
#define __WRITECOUNT__

#include <iostream>

using namespace std;

class writecount
{
	private:
		int writeid;
		int count;

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
	count = 0;
}

int writecount::increment()
{
	count++;
	return count;
}

int writecount::decrement()
{
	count--;
	return count;
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

#endif
