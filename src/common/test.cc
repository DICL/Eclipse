#include <iostream>
#include "hash.hh"

using namespace std;

int main()
{
	uint32_t result;
	result = h("abc", 6);
	cout<<result%40<<endl;
	result = h("def", 6);
	cout<<result%40<<endl;
	result = h("ghi", 6);
	cout<<result%40<<endl;
	result = h("jkl", 6);
	cout<<result%40<<endl;
	result = h("mno", 6);
	cout<<result%40<<endl;
	result = h("pqr", 6);
	cout<<result%40<<endl;
	result = h("stu", 6);
	cout<<result%40<<endl;
	result = h("vwx", 6);
	cout<<result%40<<endl;
	result = h("yzz", 6);
	cout<<result%40<<endl;
}
