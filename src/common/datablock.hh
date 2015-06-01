#ifndef __DATABLOCK__
#define __DATABLOCK__

#include <vector>
#include <string>

using namespace std;

class datablock
{
    private:
        char* data;
        unsigned size; // block size(BLOCKSIZE) is defined in common/ecfs.hh
        vector<int> recordindex; // index of start of each record
        
    public:
        datablock();
        ~datablock();
        
        int write_record (string record);   // true when succeeded, false when insufficient capacity
        bool read_record (unsigned pos, string& record);   // -1 when data reaches end of block
        char* get_data();
        unsigned get_size();
        void set_size (unsigned num);
};

#endif
