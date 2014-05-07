#ifndef __WRITECOUNT__
#define __WRITECOUNT__

#include <iostream>

using namespace std;

class writecount
{
	private:
		int writeid;

	public:
		vector<int> peerids;

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
	for(int i = 0; (unsigned)i < peerids.size(); i++)
	{
		if(peerids[i] == peerid)
			return false;
	}
	peerids.push_back(peerid);
	return true;
}

bool writecount::clear_peer(int peerid)
{
	for(int i = 0; (unsigned)i < peerids.size(); i++)
	{
		if(peerids[i] == peerid)
		{
			peerids.erase(peerids.begin()+i);
			return true;
		}
	}
	return false;
}

#endif
