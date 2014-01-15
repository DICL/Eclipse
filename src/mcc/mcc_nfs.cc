#include <iostream>
#include <mapreduce/definitions.hh>
#include "mcc_nfs.hh"

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
		cout<<"\tRemember, your program cannot use the words 'MAP', 'REDUCE' as arguments of your program."<<endl;
		cout<<"\t(If using those words as arguments is inevitable, please avoid them to be the last argument.)"<<endl;
	}

	char** argvalue = new char*[argc+3];
	argvalue[0] = "/usr/bin/g++";

	string libpath = LIB_PATH;
	libpath.append("mapreduce/nfs_hh/");

	for(int i=1;i<argc;i++)
	{
		argvalue[i] = new char[strlen(argv[i]+1)];
		strcpy(argvalue[i], argv[i]);
	}
	argvalue[argc] = new char[3];
	strcpy(argvalue[argc], "-I");

	argvalue[argc+1] = new char[strlen(libpath.c_str())+1];
	strcpy(argvalue[argc+1], libpath.c_str());

	argvalue[argc+2] = NULL;

	execv(argvalue[0], argvalue);
	return 0;
}
