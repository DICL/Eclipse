#include <iostream>
#include "hash.hh"

using namespace std;

int main()
{
	uint32_t result;
	char read_buf[32];
	char tmp_buf[32] = "all";
	string abc = "all";
	memset(read_buf, 0, 32);
	strcpy(read_buf, abc.c_str());
	result = h(read_buf, 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(abc.c_str(), 6);
	cout<<(result%20)+1<<endl;
	result = h(tmp_buf, 6);
	cout<<(result%20)+1<<endl;
	result = h("all", 6);
	cout<<(result%20)+1<<endl;
}
