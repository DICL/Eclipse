#include <iostream>
#include <vector>
#include <common/hash.hh>
#include <fstream>
#include <sstream>
#include <mapreduce/definitions.hh>
#include <common/settings.hh>

using namespace std;

int main (int argc, char** argv)
{
    // initialize data structures from nodelist.conf
    ifstream nodelistfile;
    vector<string> nodelist;
    string token;
    char buf[BUF_SIZE];

    Settings setted;
    setted.load_settings();
    nodelist = setted.nodelist();
    
    // argv[1] name of input file which includes list of input files
    // argv[2] name of output file which will match the input files and target address
    string filename;
    ifstream input;
    ofstream output;
    input.open (argv[1]);
    output.open (argv[2]);
    
    while (1)
    {
        getline (input, filename);
        
        if (input.eof())
        {
            break;
        }
        else
        {
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
