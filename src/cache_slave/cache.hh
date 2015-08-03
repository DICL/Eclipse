#ifndef __CACHE__
#define __CACHE__

#include <iostream>
#include <common/dataentry.hh>
#include <common/datablock.hh>
#include "../common/ecfs.hh"

using namespace std;

// lookup: check_being_written() should be explicitly called from a caller
// update_size() should be periodically called from super class

class cache
{
    private:
        unsigned size;
        unsigned capacity;
        vector<dataentry*> entries; // front() is the most recent entry, back() is the least recent entry
        
    public:
        cache();
        cache (unsigned num);
        dataentry* lookup (string filename);   // lookup makes the entry the most recent
        dataentry* lookup (unsigned index);   // lookup makes the entry the most recent
        
        void new_entry (dataentry* entry);   // a zero-sized entry is added as the most recent entry
        
        void update_size(); // this function should be called periodically
        unsigned get_size();
        bool try_fit_size();
};

#endif
