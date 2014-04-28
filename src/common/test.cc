#include <iostream>
#include <string.h>
#include <stdlib.h>
#include "hash.hh"

using namespace std;

int main()
{
	uint32_t result;
	char read_buf[256];
	string abc;

	abc = "all";
	memset(read_buf, 0, 256);
	strcpy(read_buf, abc.c_str());
	result = h(read_buf, 6);
	cout<<hex<<result<<endl;

	abc = "happy birthday";
	memset(read_buf, 0, 256);
	strcpy(read_buf, abc.c_str());
	result = h(read_buf, 6);
	cout<<hex<<result<<endl;

	abc = "new year";
	memset(read_buf, 0, 256);
	strcpy(read_buf, abc.c_str());
	result = h(read_buf, 6);
	cout<<hex<<result<<endl;

	abc = "zeeifjse isefjis eifjsef";
	memset(read_buf, 0, 256);
	strcpy(read_buf, abc.c_str());
	result = h(read_buf, 6);
	cout<<hex<<result<<endl;

	abc = "zee";
	memset(read_buf, 0, 256);
	strcpy(read_buf, abc.c_str());
	result = h(read_buf, 6);
	cout<<hex<<result<<endl;

	abc = "ijfeije ";
	memset(read_buf, 0, 256);
	strcpy(read_buf, abc.c_str());
	result = h(read_buf, 6);
	cout<<hex<<result<<endl;

	abc = "starcraf wjfwiejf iwejfiwje ifjweifjwief iwejfiejwifjw eifjwiefjiw efjwiefjwfwiejf wiejfwiejf iwejfiwef t";
	memset(read_buf, 0, 256);
	strcpy(read_buf, abc.c_str());
	result = h(read_buf, 6);
	cout<<hex<<result<<endl;
}
