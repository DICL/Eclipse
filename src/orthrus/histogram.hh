#ifndef __HISTOGRAM__
#define __HISTOGRAM__

#include <iostream>
#include <mapreduce/definitions.hh>

#define MAX_INT 4294967295

using namespace std;

class histogram
{
	private:
		int numbin; // number of bin -> number of nodes
		// int digit; // number of digits to represent the problem space
		unsigned* boundaries; // the index of end point of each node

	public:
		histogram(); // constructs an uninitialized object
		histogram(int num); // number of bin and number of digits
		~histogram();

		void initialize(); // partition the problem space equally to each bin
		unsigned get_boundary(int index);
		int get_index(unsigned query); // return the dedicated node index of query

		void set_numbin(int num);
		int get_numbin();
};

histogram::histogram()
{
	numbin = -1;
	boundaries = NULL;
}

histogram::histogram(int num)
{
	numbin = num;
	boundaries = new unsigned[num];
	
	this->initialize();
}

histogram::~histogram()
{
	if(boundaries != NULL)
		delete boundaries;
}

void histogram::initialize()
{
	unsigned max = MAX_INT;

	for(int i = 0; i < numbin-1; i++)
		boundaries[i] = (int)(((double)max/(double)numbin)*((double)(i+1)));

	boundaries[numbin-1] = max;
}

void histogram::set_numbin(int num)
{
	numbin = num;

	if(boundaries != NULL)
		delete boundaries;
	boundaries = new unsigned[num];
}

int histogram::get_numbin()
{
	return numbin;
}

unsigned histogram::get_boundary(int index) // the index starts from 0
{
	if(index >= numbin)
	{
		cout<<"[histogram]Index requested is out of range"<<endl;
		return -1;
	}
	else
	{
		return boundaries[index];
	}
}

int histogram::get_index(unsigned query)
{
	for(int i = 0; i < numbin; i++)
	{
		if(query <= boundaries[i])
			return i;
	}

	cout<<"[histogram]Debugging: Cannot find index of requested query."<<endl;
	return -1;
}

#endif
