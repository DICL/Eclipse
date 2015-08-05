#ifndef __IDISTRIBUTOR__
#define __IDISTRIBUTOR__

#include <iostream>
#include <sstream>
#include "writecount.hh"
#include "iwriter.hh"
#include "filepeer.hh"
#include <common/ecfs.hh>
#include <common/msgaggregator.hh>

using namespace std;

class idistributor
{
    private:
        vector<filepeer*>* peers; // node lists
        vector<msgaggregator*> msgbuffers;
        vector<iwriter*>* iwriters;
        iwriter* thewriter;
        writecount* thecount;
        int jobid;
        int networkidx;
        
    public:
        idistributor (vector<filepeer*>* apeers, vector<iwriter*>* aniwriters, writecount* acount, int ajobid, int anetworkidx);
        ~idistributor();
        
        void process_message (char* token, char * buf);
        void flush();
};

#endif
