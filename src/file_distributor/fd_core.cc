#include <iostream>
#include <vector>
#include <common/hash.hh>
#include <fstream>
#include <sstream>
#include <mapreduce/definitions.hh>

using namespace std;

int main (int argc, char** argv) {
  // initialize data structures from nodelist.conf
  ifstream nodelistfile;
  vector<string> nodelist;
  string token;
  string filepath = LIB_PATH;
  char buf[BUF_SIZE];
  filepath.append ("nodelist.conf");
  nodelistfile.open (filepath.c_str());
  
  nodelistfile >> token;
  
  while (!nodelistfile.eof()) {
    nodelist.push_back (token);
    nodelistfile >> token;
  }
  
  // argv[1] name of input file which includes list of input files
  // argv[2] name of output file which will match the input files and target address
  
  string filename;
  ifstream input;
  ofstream output;
  input.open (argv[1]);
  output.open (argv[2]);
  
  while (1) {
    getline (input, filename);
    
    if (input.eof()) {
      break;
      
    } else {
      string address;
      stringstream ss;
      
      memset (buf, 0, BUF_SIZE);
      strcpy (buf, filename.c_str());
      uint32_t hashvalue = h (buf, HASHLENGTH);
      hashvalue = hashvalue % nodelist.size();
      
      getline (input, filename);   // get full path of the file
      
      output << "scp " << filename << " " << nodelist[hashvalue] << ":" << DHT_PATH << endl;
    }
  }
  
  input.close();
  output.close();
}
