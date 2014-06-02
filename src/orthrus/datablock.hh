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

	public:
		datablock();
		~datablock();

		int write_record(string record); // true when succeeded, false when insufficient capacity
		int read_record(unsigned pos, string& record); // -1 when data reaches end of block
		char* get_data();
		unsigned get_size();
		void set_size(unsigned num);
};

datablock::datablock()
{
	data = new char[BLOCKSIZE];
	memset(data, 0, BLOCKSIZE);
	size = 0;
}

datablock::~datablock()
{
	if(data != NULL)
		delete data;
}

char* datablock::get_data()
{
	return data;
}

unsigned datablock::get_size()
{
	return size;
}

void datablock::set_size(unsigned num)
{
	size = num;
}

int datablock::write_record(string record)
{
	if(record.length() + 1 > BLOCKSIZE - size)
	{
		return -1;
	}
	else // capacity allows the data
	{
		int written_size = record.length();

		if(size > 0) // if this is not the start of block
		{
			// append newline character \n
			data[size] = '\n';
			size++;
			written_size++;
		}

		// append the contents of the record
		strcpy(data+size, record.c_str());
		size += record.length();

		return written_size;
	}
}

int datablock::read_record(unsigned pos, string& record)
{
	if(pos >= size)
	{
		return -1;
	}
	else // there is another data to read
	{
		unsigned index = pos;
		int recordsize = 0;

		while(pos < BLOCKSIZE) // scan until the (possibly) end of block
		{
			if(data[index] == '\n')
			{
				record.assign(data+pos, recordsize);
				recordsize++;
				break;
			}

			if(data[index] == 0)
			{
				record.assign(data+pos, recordsize);
				break;
			}

			recordsize++;
			index++;
		}
		return recordsize;
	}
}

#endif
