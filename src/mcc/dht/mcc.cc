#include <iostream>
#include <mapreduce/definitions.hh>
#include <common/settings.hh>
#include "mcc.hh"

using namespace std;

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        cout << "Insufficient arguments: at least 1 argument needed" << endl;
        cout << "usage: mcc [source code] (options)" << endl;
        cout << "Exiting..." << endl;
        return 1;
    }
    else
    {
        cout << "Compiling the code..." << endl;
        cout << "\tRemember followings," << endl;
        cout << "\t1. Your program cannot use the words 'MAP', 'REDUCE' as arguments of your program." << endl;
        cout << "\t (If using those words as arguments is inevitable, please avoid them to be the last argument.)" << endl;
    }
    
    char** argvalue = new char*[argc + 11];
    //argvalue[0] = "/usr/bin/g++";
    argvalue[0] = new char[strlen ("/opt/centos/devtoolset-1.1/root/usr/bin/g++") + 1];
    memset (argvalue[0], 0, strlen ("/opt/centos/devtoolset-1.1/root/usr/bin/g++") + 1);
    strcpy (argvalue[0], "/opt/centos/devtoolset-1.1/root/usr/bin/g++");
     
    Settings setted;
    setted.load_settings ();

    string strPATH = setted.lib_path ();
    string libpath = setted.lib_path ();
	string configpath = setted.lib_path ();
    string hashpath = setted.lib_path ();
	string settingpath = setted.lib_path ();
	string boostpath = "/usr/include/boost141/";

    libpath.append ("../src/mapreduce/dht/");
	strPATH.append ("../src/");
    hashpath.append ("objs/hash.o");
	settingpath.append ("objs/settings.o");
	configpath.append ("../");
    
    for (int i = 1; i < argc; i++)
    {
        argvalue[i] = new char[strlen (argv[i] + 1)];
        memset (argvalue[i], 0, strlen (argv[i]) + 1);
        strcpy (argvalue[i], argv[i]);
    }
    
    argvalue[argc] = new char[3];
    memset (argvalue[argc], 0, strlen ("-I") + 1);
    strcpy (argvalue[argc], "-I");

    argvalue[argc + 1] = new char[libpath.length() + 1];
    memset (argvalue[argc + 1], 0, libpath.length() + 1);
    strcpy (argvalue[argc + 1], libpath.c_str());

    argvalue[argc + 2] = new char[3];
    memset (argvalue[argc + 2], 0, strlen ("-I") + 1);
    strcpy (argvalue[argc + 2], "-I");

    argvalue[argc + 3] = new char[strlen (strPATH.c_str()) + 1];
    memset (argvalue[argc + 3], 0, strlen (strPATH.c_str()) + 1);
    strcpy (argvalue[argc + 3], strPATH.c_str());

    argvalue[argc + 4] = new char[hashpath.length() + 1];
    memset (argvalue[argc + 4], 0, hashpath.length() + 1);
    strcpy (argvalue[argc + 4], hashpath.c_str());

	argvalue[argc + 5] = new char[3];
	memset (argvalue[argc + 5], 0, strlen ("-I") + 1);
	strcpy (argvalue[argc + 5], "-I");

	argvalue[argc + 6] = new char[configpath.length() + 1];
	memset (argvalue[argc + 6], 0, configpath.length() + 1);
	strcpy (argvalue[argc + 6], configpath.c_str());

	argvalue[argc + 7] = new char[3];
	memset (argvalue[argc + 7], 0, strlen ("-I") + 1);
	strcpy (argvalue[argc + 7], "-I");

	argvalue[argc + 8] = new char[boostpath.length() + 1];
	memset (argvalue[argc + 8], 0, boostpath.length() + 1);
	strcpy (argvalue[argc + 8], boostpath.c_str());

    argvalue[argc + 9] = new char[settingpath.length() + 1];
    memset (argvalue[argc + 9], 0, settingpath.length() + 1);
    strcpy (argvalue[argc + 9], settingpath.c_str());

    argvalue[argc + 10] = NULL;
    execv (argvalue[0], argvalue);


    return 0;
}
