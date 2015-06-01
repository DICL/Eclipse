#ifndef __DATAENTRY__
#define __DATAENTRY__

class entrywriter;
#include <iostream>
#include <string.h>
#include "datablock.hh"

using namespace std;

class dataentry
{
    private:
        string filename;
        unsigned index;
        unsigned size;
        bool being_written;
        int lockcount;
        
    public:
        vector<datablock*> datablocks; // a member public field.
        
        dataentry (string name, unsigned idx);
        ~dataentry();
        string get_filename();
        unsigned get_index();
        unsigned get_size();
        void set_size (unsigned num);
        void lock_entry();
        void unlock_entry();
        void mark_being_written();
        void unmark_being_written();
        
        bool is_locked();
        bool is_being_written();
};


// --------entryreader--------

class entryreader
{
    private:
        dataentry* targetentry;
        int blockindex;
        unsigned index;
        
    public:
        entryreader();
        entryreader (dataentry* entry);
        void set_targetentry (dataentry* entry);
        bool read_record (string& record);   // return false when it reaches end of data
};

// --------entrywriter--------

class entrywriter
{
    private:
        dataentry* targetentry;
        
    public:
        entrywriter();
        entrywriter (dataentry* entry);
        void set_targetentry (dataentry* entry);
        bool write_record (string record);
        void complete(); // unlock the entry and unmark as it is not being written
};

#endif
