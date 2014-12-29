#include <iostream>
#include <vector>
#include <common/hash.hh>
#include <fstream>
#include <sstream>
#include <mapreduce/definitions.hh>

using namespace std;

int main (int argc, char** argv)
{
    char filename[BUF_SIZE];
    
    if (argc != 2)
    {
        cout << "Usage: mrrm [input name]" << endl;
        return 0;
    }
    
    // initialize data structures from nodelist.conf
    ifstream nodelistfile;
    vector<string> nodelist;
    string token;
    string filepath = LIB_PATH;
    filepath.append ("nodelist.conf");
    nodelistfile.open (filepath.c_str());
    nodelistfile >> token;
    
    while (!nodelistfile.eof())
    {
        nodelist.push_back (token);
        nodelistfile >> token;
    }
    
    memset (filename, 0, BUF_SIZE);
    strcpy (filename, argv[1]);
    string outputfilename = MR_PATH;
    outputfilename.append ("mrrm.sh");
    ofstream output;
    output.open (outputfilename.c_str());
    uint32_t hashvalue = h (filename, HASHLENGTH);
    hashvalue = hashvalue % nodelist.size();
    output << "ssh " << nodelist[hashvalue] << " rm -f " << DHT_PATH << filename << endl;
    output.close();
}
