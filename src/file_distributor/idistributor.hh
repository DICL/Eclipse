#ifndef __IDISTRIBUTOR__
#define __IDISTRIBUTOR__

#include <iostream>
#include <sstream>
#include "writecount.hh"
#include "iwriter.hh"
#include <mapreduce/definitions.hh>
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

idistributor::idistributor (vector<filepeer*>* apeers, vector<iwriter*>* aniwriters, writecount* acount, int ajobid, int anetworkidx)
{
    peers = apeers;
    iwriters = aniwriters;
    thewriter = NULL;
    thecount = acount;
    jobid = ajobid;
    networkidx = anetworkidx;
    
    // set up initial configuration of message buffers
    for (int i = 0; (unsigned) i < peers->size(); i++)
    {
        stringstream ss;
        ss << "Iwrite ";
        ss << jobid;
        ss << "\n";
        
        if (i == networkidx)
        {
            msgbuffers.push_back (NULL);
        }
        else
        {
            msgbuffers.push_back (new msgaggregator ( (*peers) [i]->get_fd()));
            msgbuffers[i]->configure_initial (ss.str());   // Iwrite [job id] [node index]\n[file name]\n[record] ...
            msgbuffers[i]->set_msgbuf (& (*peers) [i]->msgbuf);
        }
    }
}

idistributor::~idistributor()
{
    // flush remaining messages
    flush();
    
    // clear up message buffers
    for (int i = 0; (unsigned) i < peers->size(); i++)
    {
        if (i != networkidx)
        {
            delete msgbuffers[i];
        }
    }
}

void idistributor::process_message (char* token, char* buf)
{
    string key;
    token = strtok (token,  " ");   // tokenize first key
    
    while (token != NULL)
    {
        key = token;
        token = strtok (NULL, "\n");   // tokenize value
        memset (buf, 0, HASHLENGTH);
        strcpy (buf, key.c_str());
        uint32_t hashvalue = h (buf, HASHLENGTH);
        hashvalue = hashvalue % (peers->size());
        
        if (hashvalue == (unsigned) networkidx)
        {
            if (thewriter == NULL)
            {
                // write to the local iwriter
                for (int i = 0; (unsigned) i < iwriters->size(); i++)
                {
                    if ( (*iwriters) [i]->get_jobid() == jobid)
                    {
                        thewriter = (*iwriters) [i];
                        break;
                    }
                }
                
                if (thewriter == NULL)     // no iwriter with the job id
                {
                    // create new iwriter
                    thewriter = new iwriter (jobid, networkidx);
                    iwriters->push_back (thewriter);
                }
            }
            
            // add key value pair to the iwriter
            thewriter->add_keyvalue (key, token);
            thecount->add_peer (hashvalue);
        }
        else
        {
            key.append (" ");
            key.append (token);
            msgbuffers[hashvalue]->add_record (key);
        }
        
        thecount->add_peer (hashvalue);
        token = strtok (NULL, " ");   // next key
    }
}

void idistributor::flush()
{
    // flush all message aggregator
    for (int i = 0; (unsigned) i < peers->size(); i++)
    {
        if (i != networkidx)
        {
            msgbuffers[i]->flush();
        }
    }
}

#endif
