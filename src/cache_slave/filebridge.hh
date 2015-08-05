#ifndef __FILEBRIDGE__
#define __FILEBRIDGE__

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <common/ecfs.hh>
#include "../common/dataentry.hh"
#include "filepeer.hh"
#include "writecount.hh"
#include "file_connclient.hh"

using namespace std;

class filebridge
{
    private:
        int id; // this id will be sent to peer to link this filebridge
        int dstid; // the bridge id of remote peer, -1 as default, positive value when the dsttype is PEER

        bridgetype srctype; // PEER, DISK, CACHE or CLIENT
        bridgetype dsttype; // PEER, DISK, CACHE or CLIENT
        filepeer* dstpeer; // destination peer
        file_connclient* dstclient; // destination client
        idistributor* theidistributor;
        entrywriter* dstentrywriter; // when this is not null, data should be written to the entry
        entryreader* srcentryreader; // when the srctype is CACHE, srcentryreader should be set and it should read data from cache
        fstream readfilestream;
        
        string Icachekey;
        string jobdirpath; // used for the distribution of Icache contents
        string dht_path;
        
    public:
        writecount* thecount; // list of nodes the to which the outputs(RAW, INTERMEDIATE) are transffered
        msgaggregator* writebuffer;
        
        filebridge (int anid);
        ~filebridge();
        
        void set_dstpeer (filepeer* apeer);
        //void set_role(file_role arole);
        void set_srctype (bridgetype atype);
        void set_dsttype (bridgetype atype);
        void set_id (int num);
        void set_dstid (int num);
        void set_dstclient (file_connclient* aclient);
        void set_entryreader (entryreader* areader);
        void set_entrywriter (entrywriter* awriter);
        void set_distributor (idistributor* thedistributor);
        
        idistributor* get_distributor();
        
        void set_jobdirpath (string path);
        string get_jobdirpath();
        
        void set_Icachekey (string key);
        string get_Icachekey();
        
        int get_id();
        int get_dstid();
        filepeer* get_dstpeer();
        
        file_connclient* get_dstclient();
        bridgetype get_srctype();
        bridgetype get_dsttype();
        entryreader* get_entryreader();
        entrywriter* get_entrywriter();
        
        void open_readfile (string fname);
        bool read_record (string& record);   // reads next record
};

#endif
