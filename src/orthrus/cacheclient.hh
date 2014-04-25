#ifndef __CACHECLIENT__
#define __CACHECLIENT__

#include <iostream>

using namespace std;

class cacheclient
{
	private:
		int fd;
		string address;

	public:
		cacheclient(int number, string anaddress);
		int get_fd();
		void set_fd(int num);
		string get_address(); 
		void set_address(string anaddress); 
};

cacheclient::cacheclient(int number, string anaddress)
{
	fd = number;
	address = anaddress;
}

int cacheclient::get_fd()
{
	return fd;
}

void cacheclient::set_fd(int num)
{
	fd = num;
}

string cacheclient::get_address()
{
	return address;
}

void cacheclient::set_address(string anaddress)
{
	address = anaddress; 
}

#endif
