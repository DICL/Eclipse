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

filebridge::filebridge (int anid)
{
    id = anid;
    dstid = -1;
    //writefilefd = -1;
    thecount = NULL;
    dstpeer = NULL;
    dstclient = NULL;
    srcentryreader = NULL;
    dstentrywriter = NULL;
    writebuffer = NULL;
    theidistributor = NULL;
    //keybuffer.configure_initial("Ikey\n");

    Settings setted;
    setted.load();
    dht_path = setted.get<string>("path.scratch");
}

filebridge::~filebridge()
{
    // clear up all things
    if (thecount != NULL)
    {
        delete thecount;
    }
    
    readfilestream.close();
    
    if (writebuffer != NULL)
    {
        delete writebuffer;
    }
    
    //close(this->writefilefd);
}

void filebridge::set_entryreader (entryreader* areader)
{
    srcentryreader = areader;
}

void filebridge::set_entrywriter (entrywriter* awriter)
{
    dstentrywriter = awriter;
}

void filebridge::set_dstpeer (filepeer* apeer)
{
    this->dstpeer = apeer;
}

void filebridge::set_distributor (idistributor* thedistributor)
{
    theidistributor = thedistributor;
}

idistributor* filebridge::get_distributor()
{
    return theidistributor;
}

filepeer* filebridge::get_dstpeer()
{
    return dstpeer;
}

entryreader* filebridge::get_entryreader()
{
    return srcentryreader;
}

entrywriter* filebridge::get_entrywriter()
{
    return dstentrywriter;
}

void filebridge::open_readfile (string fname)
{
    string fpath = dht_path;
    fpath += "/"; // Vicente (this solves critical error)
    fpath.append (fname);
    this->readfilestream.open (fpath.c_str());
    
    if (!this->readfilestream.is_open())
    {
        cout << "[filebridge]File does not exist for reading: " << fname << endl;
    }
    
    return;
}

bool filebridge::read_record (string& record)     // reads next record
{
    getline (this->readfilestream, record);
    
    if (this->readfilestream.eof())
    {
        return false;
    }
    else
    {
        return true;
    }
}

file_connclient* filebridge::get_dstclient()
{
    return this->dstclient;
}

void filebridge::set_srctype (bridgetype atype)
{
    this->srctype = atype;
}

void filebridge::set_dsttype (bridgetype atype)
{
    this->dsttype = atype;
}

bridgetype filebridge::get_srctype()
{
    return this->srctype;
}

bridgetype filebridge::get_dsttype()
{
    return this->dsttype;
}

int filebridge::get_id()
{
    return id;
}

int filebridge::get_dstid()
{
    return dstid;
}

void filebridge::set_id (int num)
{
    id = num;
}

void filebridge::set_dstid (int num)
{
    dstid = num;
}

//file_role filebridge::get_role()
//{
//  return this->dstclient->get_role();
//}

void filebridge::set_dstclient (file_connclient* aclient)
{
    this->dstclient = aclient;
}
void filebridge::set_Icachekey (string key)
{
    Icachekey = key;
}

string filebridge::get_Icachekey()
{
    return Icachekey;
}

void filebridge::set_jobdirpath (string path)
{
    jobdirpath = path;
}

string filebridge::get_jobdirpath()
{
    return jobdirpath;
}

#endif
