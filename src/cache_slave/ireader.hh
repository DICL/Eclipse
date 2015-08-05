#ifndef __IREADER__
#define __IREADER__

#include <iostream>
#include <fstream>
#include <sstream>
#include "filepeer.hh"
#include "file_connclient.hh"
#include <common/ecfs.hh>
#include "../common/settings.hh"

using namespace std;

class ireader
{
    private:
        int jobid;
        int pos;
        int numiblock;
        int networkidx;
        int bridgeid;
        int readingfile;
        int finishedcount;
        
        string filename;
        filepeer* dstpeer;
        file_connclient* dstclient;
        
        char write_buf[BUF_SIZE];
        string dht_path;
        
        vector<ifstream*> files;
        vector<string> filepaths;
        vector<string> currentkeys;
        vector<int> remaining_record;
        
        string currentkey;
        
        map<string, vector<int>*> keyorder;
        
        bridgetype dsttype; // PEER, DISK, CACHE or CLIENT
        
        int get_jobid();
        int get_numiblock();
        file_connclient* get_dstclient();
        filepeer* get_dstpeer();
        void set_bridgeid (int anid);
        
    public:
        ireader (int ajobid, int anumiblock, int anetworkidx, int abridgeid, bridgetype adsttype);
        void set_dstpeer (filepeer* apeer);
        void set_dstclient (file_connclient* aclient);
        bool read_idata();
};

#endif
