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
    char *filename;
    filename = (char*)malloc(BUF_SIZE);
    
    if (argc != 2)
    {
        cout << "Usage: mrrm [input name]" << endl;
        return 0;
    }
    
    // initialize data structures from nodelist.conf
    ifstream nodelistfile;
    vector<string> nodelist;

    Settings setted;
    setted.load_settings();
    nodelist = setted.nodelist();
    
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
