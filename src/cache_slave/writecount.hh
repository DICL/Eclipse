#ifndef __WRITECOUNT__
#define __WRITECOUNT__

#include <iostream>
#include <set>

using namespace std;

class writecount
{
    private:
    public:
        set<int> peerids;
        
        writecount();
        int size();
        bool add_peer (int peerid);
        bool clear_peer (int peerid);
};

#endif
