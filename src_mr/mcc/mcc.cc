#include <iostream>
#include <mapreduce/mapreduce.hh>
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

	char** argvalue = new char*[argc+2];
	argvalue[0] = "/usr/bin/g++";

	for(int i=1;i<argc;i++)
	{
		argvalue[i] = new char[sizeof(argv[i])];
		strcpy(argvalue[i], argv[i]);
	}
	argvalue[argc] = "-I";
	argvalue[argc+1] = "/home/youngmoon01/MRR/MRR/src_mr/";


	execv("/usr/bin/g++", argvalue);
	return 0;
}
