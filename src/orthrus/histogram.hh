#ifndef __HISTOGRAM__
#define __HISTOGRAM__

#include <iostream>
#include <mapreduce/definitions.hh>

using namespace std;

class histogram
{
	private:
		int numbin; // number of bin -> number of nodes
		int digit; // number of digits to represent the problem space
		int* boundaries; // the index of end point of each node

	public:
		histogram(); // constructs an uninitialized object
		histogram(int num, int digit); // number of bin and number of digits
		~histogram();

		void initialize(); // partition the problem space equally to each bin
		int get_boundary(int index);
		int get_index(int query); // return the dedicated node index of query

		void set_numbin(int num);
		int get_numbin();
		void set_digit(int num);
		int get_digit();
};

histogram::histogram()
{
	numbin = -1;
	boundaries = NULL;
}

histogram::histogram(int num, int digit)
{
	numbin = num;
	boundaries = new int[num];
	
	this->initialize();
}

histogram::~histogram()
{
	if(boundaries != NULL)
		delete boundaries;
}

void histogram::initialize()
{
	int max = 1;
	for(int i = 0; i < digit; i++)
		max *= 10;
	max -= 1; // max <- 9999 when digit=4

	for(int i = 0; i < numbin-1; i++)
		boundaries[i] = (int)(((double)max/(double)numbin)*((double)(i+1)));

	boundaries[numbin-1] = max;
}

void histogram::set_numbin(int num)
{
	numbin = num;

	if(boundaries != NULL)
		delete boundaries;
	boundaries = new int[num];
}

int histogram::get_numbin()
{
	return numbin;
}

void histogram::set_digit(int num)
{
	digit = num;
}

int histogram::get_digit()
{
	return digit;
}

int histogram::get_boundary(int index) // the index starts from 0
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

int histogram::get_index(int query)
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
