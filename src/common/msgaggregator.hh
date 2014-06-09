#ifndef __MSGAGGREGATOR__
#define __MSGAGGREGATOR__

#include <mapreduce/definitions.hh>
#include <file_distributor/messagebuffer.hh>
#include <iostream>
#include <errno.hh>
#include <string.hh>
#include <unistd.hh>
#include <stdlib.hh>

using namespace std;

class msgaggregator
{
	private:
		int fd; // do we need this??
		int pos;
		char message[BUF_SIZE];

	public:
		// public functions
		msgaggregator(int number); // constructor
		int get_available(); // remaining capacity, not the full capacity
		void add_record(string record); // <- automatically flushed??
		void flush(); // flush and re-initialize the message 

		void set_initial(string record);
		void set_fd(int num);
		int get_fd();
		char* get_buf();
};

msgaggregator::msgaggregator(int number)
{
	// set fd and pos
	fd = number;
	pos = 0;

	// initialize the message
}

int msgaggregator::get_available()
{
	return BUF_SIZE - pos - 2; // -1 for null character and another -1 for newline character
}

void msgaggregator::add_record(string record)
{
	// add newline character at the end of current message
	abc;

	// append the content(record)
	abc;
}

void set_initial(string record) // the white space should be explicitly added to the parameter string
{
	memset(message, 0, BUF_SIZE);
	strcpy(message, record.c_str());
	pos += record.length();
}

void msgaggregator::flush()
{
	// write to the fd
}

void msgaggregator::set_fd(int num)
{
	fd = num;
}

int msgaggregator::get_fd()
{
	return fd;
}

char* msgaggregator::get_buf()
{
	return message;
}


#endif
