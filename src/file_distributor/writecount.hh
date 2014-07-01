#ifndef __WRITECOUNT__
#define __WRITECOUNT__

#include <iostream>
#include <set>

using namespace std;

class writecount
{
	private:
		int writeid;

	public:
		set<int> peerids;

		writecount(int anid);
		int get_id();
		void set_id(int num);
		bool add_peer(int peerid);
		bool clear_peer(int peerid);
};

writecount::writecount(int anid)
{
	writeid = anid;
}

int writecount::get_id()
{
	return writeid;
}

void writecount::set_id(int num)
{
	writeid = num;
}

bool writecount::add_peer(int peerid)
{
	if(peerids.find(peerid) == peerids.end())
	{
		peerids.insert(peerid);
		return true;
	}
	return false;

}

bool writecount::clear_peer(int peerid)
{
	int ret = peerids.erase(peerid);
	if(ret == 1)
		return true;
	else
		return false;
}

#endif
