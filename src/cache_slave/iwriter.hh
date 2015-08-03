#ifndef __IWRITER__
#define __IWRITER__

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <common/ecfs.hh>
#include "../common/settings.hh"

using namespace std;

class iwriter
{
    private:
        char write_buf[BUF_SIZE];
        bool iswriting;
        int jobid;
        int networkidx;
        int numblock;
        int filefd;
        int vectorindex;
        
        vector<map<string, vector<string>*>*> themaps;
        
        int writingblock;
        int availableblock;
        int pos; // position in the write_buf
        map<string, vector<string>*>::iterator writing_it;
        string filepath;
        
        long currentsize;
        string dht_path;
        
    public:
        iwriter (int ajobid, int anetworkidx);
        ~iwriter();
        
        int get_jobid();
        int get_numblock();
        void add_keyvalue (string key, string value);
        bool is_writing();
        bool write_to_disk(); // return true if write_to_disk should be stopped, return false if write should be continued
        void flush();
};

#endif
