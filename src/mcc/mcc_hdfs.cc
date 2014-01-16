#include <iostream>
#include <mapreduce/definitions.hh>
#include "mcc_hdfs.hh"

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

	char** argvalue = new char*[argc+11];
	argvalue[0] = "/usr/bin/g++";

	string libpath = LIB_PATH;
	libpath.append("mapreduce/hdfs_hh/");

	for(int i=1;i<argc;i++)
	{
		argvalue[i] = new char[strlen(argv[i]+1)];
		strcpy(argvalue[i], argv[i]);
	}
	argvalue[argc] = new char[3];
	strcpy(argvalue[argc], "-I");

	argvalue[argc+1] = new char[strlen(libpath.c_str())+1];
	strcpy(argvalue[argc+1], libpath.c_str());

	argvalue[argc+2] = new char[3];
	strcpy(argvalue[argc+2], "-I");

	argvalue[argc+3] = new char[strlen(HDFS_PATH)+1];
	strcpy(argvalue[argc+3], HDFS_PATH);

	argvalue[argc+4] = new char[3];
	strcpy(argvalue[argc+4], "-L");

	argvalue[argc+5] = new char[strlen(HDFS_LIB)+1];
	strcpy(argvalue[argc+5], HDFS_LIB);

	argvalue[argc+6] = new char[3];
	strcpy(argvalue[argc+6], "-L");

	argvalue[argc+7] = new char[strlen(JAVA_LIB)+1];
	strcpy(argvalue[argc+7], JAVA_LIB);

	argvalue[argc+8] = new char[strlen(HADOOP_FLAG)+1];
	strcpy(argvalue[argc+8], HADOOP_FLAG);

	argvalue[argc+9] = new char[strlen(JAVA_FLAG)+1];
	strcpy(argvalue[argc+9], JAVA_FLAG);

	argvalue[argc+10] = NULL;

	execv(argvalue[0], argvalue);
	return 0;
}
