#include "dataentry.hh"
#include <stdlib.h>

dataentry::dataentry (string name, unsigned idx)
{
    filename = name;
    index = idx;
    size = 0;
    lockcount = 0;
    being_written = false;
}

dataentry::~dataentry()
{
    for (int i = 0; (unsigned) i < datablocks.size(); i++)
    {
        delete datablocks[i];
    }
}

string dataentry::get_filename()
{
    return filename;
}

unsigned dataentry::get_index()
{
    return index;
}

unsigned dataentry::get_size()
{
    return size;
}

void dataentry::set_size (unsigned num)
{
    size = num;
}

void dataentry::lock_entry()
{
    lockcount++;
}

void dataentry::unlock_entry()
{
    lockcount--;
}

void dataentry::mark_being_written()
{
    being_written = true;
}

void dataentry::unmark_being_written()
{
    being_written = false;
}

bool dataentry::is_locked()
{
    if (lockcount > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool dataentry::is_being_written()
{
    return being_written;
}

entryreader::entryreader()
{
    targetentry = NULL;
    blockindex = 0;
    index = 0;
}

entryreader::entryreader (dataentry* entry)
{
    targetentry = entry;
    blockindex = 0;
    index = 0;
    entry->lock_entry();
}

void entryreader::set_targetentry (dataentry* entry)
{
    targetentry = entry;
    blockindex = 0;
    index = 0;
    entry->lock_entry();
}

bool entryreader::read_record (string& record)
{
    if (targetentry->datablocks[blockindex]->read_record (index, record))       // a record successfully read
    {
        index++;
        return true;
    }
    else     // no more data in current block
    {
        blockindex++;
        index = 0;
        
        if ( (unsigned) blockindex < targetentry->datablocks.size())      // next block exist
        {
            if (!targetentry->datablocks[blockindex]->read_record (index, record))       // first read must succeed from next block
            {
                cout << "[entryreader]Debugging: Unexpected response from read_record()." << endl;
                exit (1);
            }
            
            index++;
            return true;
        }
        else     // no more next block
        {
            targetentry->unlock_entry();
            return false;
        }
    }
}

entrywriter::entrywriter()
{
    targetentry = NULL;
}

entrywriter::entrywriter (dataentry* entry)
{
    targetentry = entry;
    entry->lock_entry();
    entry->mark_being_written();
}

void entrywriter::set_targetentry (dataentry* entry)
{
    targetentry = entry;
    entry->lock_entry();
    entry->mark_being_written();
}

bool entrywriter::write_record (string record)
{
    if (targetentry->datablocks.size() == 0)   // if no block exist for this entry
    {
        targetentry->datablocks.push_back (new datablock());
    }
    
    int ret = targetentry->datablocks.back()->write_record (record);
    
    if (ret < 0)     // record doesn't fit into the current block
    {
        targetentry->datablocks.push_back (new datablock());
        ret = targetentry->datablocks.back()->write_record (record);
        
        if (ret < 0)     // first write must succeed from new block
        {
            cout << "[entrywriter]Debugging: Unexpected response from write_record()." << endl;
            exit (1);
        }
        
        targetentry->set_size (targetentry->get_size() + ret);
    }
    else     // the record successfully written
    {
        targetentry->set_size (targetentry->get_size() + ret);
    }
    
    return true;
}

void entrywriter::complete()
{
    targetentry->unlock_entry();
    targetentry->unmark_being_written();
}
