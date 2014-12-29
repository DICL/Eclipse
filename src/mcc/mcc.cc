#include <iostream>
#include <mapreduce/definitions.hh>
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
    cout << "\tRemember, your program cannot use the words 'MAP', 'REDUCE' as arguments of your program." << endl;
    cout << "\t(If using those words as arguments is inevitable, please avoid them to be the last argument.)" << endl;
  }
  
  char** argvalue = new char*[argc + 2];
  argvalue[0] = "/usr/bin/g++";
  
  for (int i = 1; i < argc; i++)
  {
    argvalue[i] = new char[sizeof (argv[i])];
    strcpy (argvalue[i], argv[i]);
  }
  
  argvalue[argc] = "-I";
  argvalue[argc + 1] = LIB_PATH;
  
  
  execv ("/usr/bin/g++", argvalue);
  return 0;
}
