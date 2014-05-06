#ifndef __DATAENTRY__
#define __DATAENTRY__

#include <iostream>
#include <string.h>
#include <orthrus/datablock.hh>

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

		dataentry(string name, unsigned idx);
		~dataentry();
		string get_filename();
		unsigned get_index();
		unsigned get_size();
		void set_size(unsigned num);
		void lock_entry();
		void unlock_entry();
		void mark_being_written();
		void unmark_being_written();

		bool is_locked();
		bool is_being_written();
};

dataentry::dataentry(string name, unsigned idx)
{
	filename = name;
	index = idx;
	size = 0;
	lockcount = 0;
	being_written = false;
}

dataentry::~dataentry()
{
	for(int i = 0; (unsigned)i < datablocks.size(); i++)
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

void dataentry::set_size(unsigned num)
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
	if(lockcount > 0)
		return true;
	else
		return false;
}

bool dataentry::is_being_written()
{
	return being_written;
}

// --------entryreader--------

class entryreader
{
	private:
		dataentry* targetentry;
		int blockindex;
		unsigned pos;

	public:
		entryreader();
		entryreader(dataentry* entry);
		void set_targetentry(dataentry* entry);
		bool read_record(char* buf); // return false when it reaches end of data
};

entryreader::entryreader()
{
	targetentry = NULL;
	blockindex = 0;
	pos = 0;
}

entryreader::entryreader(dataentry* entry)
{
	targetentry = entry;
	blockindex = 0;
	pos = 0;

	entry->lock_entry();
}

void entryreader::set_targetentry(dataentry* entry)
{
	targetentry = entry;
	blockindex = 0;
	pos = 0;

	entry->lock_entry();
}

bool entryreader::read_record(char* buf)
{
	int ret = targetentry->datablocks[blockindex]->read_record(pos, buf);

	if(ret < 0) // no more data in current block
	{
		blockindex++;
		pos = 0;

		if((unsigned)blockindex < targetentry->datablocks.size()) // next block exist
		{
			ret = targetentry->datablocks[blockindex]->read_record(pos, buf);
			if(ret < 0) // first read must succeed from next block
			{
				cout<<"[entryreader]Debugging: Unexpected response from read_record()."<<endl;
				exit(1);
			}

			pos += ret;
			return true;
		}
		else // no more next block
		{
			targetentry->unlock_entry();
			return false;
		}
	}
	else // a record successfully read
	{
		pos += ret;
		return true;
	}
}

// --------entrywriter--------

class entrywriter
{
	private:
		dataentry* targetentry;

	public:
		entrywriter();
		entrywriter(dataentry* entry);
		void set_targetentry(dataentry* entry);
		bool write_record(string& record);
		void complete(); // unlock the entry and unmark as it is not being written
};

entrywriter::entrywriter()
{
	targetentry = NULL;
}

entrywriter::entrywriter(dataentry* entry)
{
	targetentry = entry;

	entry->lock_entry();
	entry->mark_being_written();
}

void entrywriter::set_targetentry(dataentry* entry)
{
	targetentry = entry;

	entry->lock_entry();
	entry->mark_being_written();
}

bool entrywriter::write_record(string& record)
{
	if(targetentry->datablocks.size() == 0) // if no block exist for this entry
		targetentry->datablocks.push_back(new datablock());

	int ret = targetentry->datablocks.back()->write_record(record);

	if(ret < 0) // record doesn't fit into the current block
	{
		targetentry->datablocks.push_back(new datablock());

		ret = targetentry->datablocks.back()->write_record(record);
		if(ret < 0) // first write must succeed from new block
		{
			cout<<"[entrywriter]Debugging: Unexpected response from write_record()."<<endl;
			exit(1);
		}

		targetentry->set_size(targetentry->get_size() + ret);
	}
	else // the record successfully written
	{
		targetentry->set_size(targetentry->get_size() + ret);
	}

	return true;
}

void entrywriter::complete()
{
	targetentry->unlock_entry();
	targetentry->unmark_being_written();
}

#endif
