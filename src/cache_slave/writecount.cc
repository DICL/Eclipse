#include "writecount.hh"

writecount::writecount()
{
    // do nothing
}

bool writecount::add_peer (int peerid)
{
    if (peerids.find (peerid) == peerids.end())
    {
        peerids.insert (peerid);
        return true;
    }
    
    return false;
}

bool writecount::clear_peer (int peerid)
{
    int ret = peerids.erase (peerid);
    
    if (ret == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int writecount::size()
{
    return peerids.size();
}


