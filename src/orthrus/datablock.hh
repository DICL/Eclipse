#ifndef __DATABLOCK__
#define __DATABLOCK__

#include <iostream>
#include <mapreduce/definitions.hh>
#include <string.h>

using namespace std;

class datablock
{
    private:
        char* data;
        unsigned size; // block size(BLOCKSIZE) is defined in mapreduce/definitions.hh
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

datablock::datablock()
{
    data = new char[BLOCKSIZE];
    memset (data, 0, BLOCKSIZE);
    size = 0;
    // add 0 index for the start of first record
    recordindex.push_back (0);
}

datablock::~datablock()
{
    if (data != NULL)
    {
        delete data;
    }
}

char* datablock::get_data()
{
    return data;
}

unsigned datablock::get_size()
{
    return size;
}

void datablock::set_size (unsigned num)
{
    size = num;
}

int datablock::write_record (string record)
{
    if (record.length() + 1 > BLOCKSIZE - size)
    {
        return -1;
    }
    else     // capacity allows the data
    {
        int written_size = record.length();
        
        if (size > 0)     // if this is not the start of block
        {
            // append newline character \n
            data[size] = '\n';
            size++;
            written_size++;
        }
        
        // append the contents of the record
        strcpy (data + size, record.c_str());
        size += record.length();
        // add start index of next record(end index of current record)
        recordindex.push_back (size + 1);
        return written_size;
    }
}

bool datablock::read_record (unsigned index, string& record)     // return next index if there is next record, or -1
{
    if (index < recordindex.size() - 1)     // there is another data to read
    {
        record.assign (data + recordindex[index], recordindex[index + 1] - recordindex[index] - 1);
        return true;
    }
    else
    {
        return false;
    }
}

#endif
