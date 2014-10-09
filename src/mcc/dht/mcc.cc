#include <iostream>
#include <mapreduce/definitions.hh>
#include "mcc.hh"

using namespace std;

int main(int argc, char** argv)
{
	if(argc<2)
	{
		cout<<"Insufficient arguments: at least 1 argument needed"<<endl;
		cout<<"usage: mcc [source code] (options)"<<endl;
		cout<<"Exiting..."<<endl;
		return 1;
	}
	else
	{
		cout<<"Compiling the code..."<<endl;
		cout<<"\tRemember followings,"<<endl;
		cout<<"\t1. Your program cannot use the words 'MAP', 'REDUCE' as arguments of your program."<<endl;
		cout<<"\t (If using those words as arguments is inevitable, please avoid them to be the last argument.)"<<endl;
	}

	char** argvalue = new char*[argc+6];
	//argvalue[0] = "/usr/bin/g++";
	argvalue[0] = new char[strlen("/usr/bin/g++") + 1];
	memset(argvalue[0], 0, strlen("/usr/bin/g++") + 1);
	strcpy(argvalue[0], "/usr/bin/g++");

	string libpath = LIB_PATH;
	libpath.append("mapreduce/dht/");
	string hashpath = LIB_PATH;
	hashpath.append("common/hash.o");

	for(int i=1;i<argc;i++)
	{
		argvalue[i] = new char[strlen(argv[i] + 1)];
		memset(argvalue[i], 0, strlen(argv[i]) + 1);
		strcpy(argvalue[i], argv[i]);
	}
	argvalue[argc] = new char[3];
	memset(argvalue[argc], 0, strlen("-I") + 1);
	strcpy(argvalue[argc], "-I");

	argvalue[argc+1] = new char[libpath.length() + 1];
	memset(argvalue[argc+1], 0, strlen(libpath.c_str()) + 1);
	strcpy(argvalue[argc+1], libpath.c_str());

	argvalue[argc+2] = new char[3];
	memset(argvalue[argc+2], 0, strlen("-I") + 1);
	strcpy(argvalue[argc+2], "-I");

	argvalue[argc+3] = new char[strlen(LIB_PATH) + 1];
	memset(argvalue[argc+3], 0, strlen(LIB_PATH) + 1);
	strcpy(argvalue[argc+3], LIB_PATH);

	argvalue[argc+4] = new char[hashpath.length()+1];
	memset(argvalue[argc+4], 0, strlen(hashpath.c_str()) + 1);
	strcpy(argvalue[argc+4], hashpath.c_str());

	argvalue[argc+5] = NULL;

	execv(argvalue[0], argvalue);
	return 0;
}
