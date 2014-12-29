#ifndef __FILEBRIDGE__
#define __FILEBRIDGE__

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <mapreduce/definitions.hh>
#include <orthrus/dataentry.hh>
#include "filepeer.hh"
#include "writecount.hh"
#include "file_connclient.hh"

using namespace std;

class filebridge
{
  private:
    int id; // this id will be sent to peer to link this filebridge
    int dstid; // the bridge id of remote peer, -1 as default, positive value when the dsttype is PEER
    //int writefilefd; // deprecated
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
    
    //char read_buf[BUF_SIZE]; // this is not for reading message from other connection, but for buffering reading file
    //string dataname; // the key of the data which is used as input of hash function
    //string filename; // the key of the data which is used as input of hash function
    //datatype dtype; // RAW, INTERMEDIATE, OUTPUT
    
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
    //void set_dataname(string aname);
    //void set_filename(string aname);
    //void set_dtype(datatype atype);
    //void set_writeid(int num);
    
    int get_id();
    int get_dstid();
    filepeer* get_dstpeer();
    //int get_writeid();
    
    //file_role get_role();
    file_connclient* get_dstclient();
    bridgetype get_srctype();
    bridgetype get_dsttype();
    entryreader* get_entryreader();
    entrywriter* get_entrywriter();
    //datatype get_dtype();
    //string get_dataname();
    //string get_filename();
    
    void open_readfile (string fname);
    //void open_writefile(string fname); // deprecated
    //void write_record(string& record, char* write_buf); // deprecated
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
}

filebridge::~filebridge()
{
  // clear up all things
  if (thecount != NULL)
    delete thecount;
    
  readfilestream.close();
  
  if (writebuffer != NULL)
    delete writebuffer;
    
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

//void filebridge::set_role(file_role arole)
//{
//  if(this->dstclient == NULL)
//  {
//    cout<<"[filebridge]The destination client of a filebridge is not set and modifying the member client tried"<<endl;
//    return;
//  }
//  this->dstclient->set_role(arole);
//}

//void filebridge::set_dataname(string aname)
//{
//  this->dataname = aname;
//}

//void filebridge::set_dtype(datatype atype)
//{
//  this->dtype = atype;
//}

filepeer* filebridge::get_dstpeer()
{
  return dstpeer;
}

//string filebridge::get_dataname()
//{
//  return dataname;
//}

//datatype filebridge::get_dtype()
//{
//  return dtype;
//}

//void filebridge::set_filename(string aname)
//{
//  this->filename = aname;
//}

//string filebridge::get_filename()
//{
//  return this->filename;
//}

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
  string fpath = DHT_PATH;
  fpath.append (fname);
  
  this->readfilestream.open (fpath.c_str());
  
  if (!this->readfilestream.is_open())
    cout << "[filebridge]File does not exist for reading: " << fname << endl;
    
  return;
}

bool filebridge::read_record (string& record)     // reads next record
{
  getline (this->readfilestream, record);
  
  if (this->readfilestream.eof())
    return false;
    
  else
    return true;
}

/*
// deprecated
void filebridge::open_writefile(string fname)
{
  string fpath = DHT_PATH;
  fpath.append(fname);
  this->writefilefd = open(fpath.c_str(), O_APPEND|O_SYNC|O_WRONLY|O_CREAT, 0644);
  if(this->writefilefd < 0)
    cout<<"filebridge]Opening write file failed"<<endl;
  return;
}
*/

/*
void filebridge::write_record(string& record, char* write_buf)
{
  struct flock alock;
  struct flock ulock;

  // set lock
  alock.l_type = F_WRLCK;
  alock.l_start = 0;
  alock.l_whence = SEEK_SET;
  alock.l_len = 0;

  // set unlock
  ulock.l_type = F_UNLCK;
  ulock.l_start = 0;
  ulock.l_whence = SEEK_SET;
  ulock.l_len = 0;


  // acquire file lock
  fcntl(this->writefilefd, F_SETLKW, &alock);

  // critical section
  {
    int ret;
    record.append("\n");
    memset(write_buf, 0, BUF_SIZE);
    strcpy(write_buf, record.c_str());
    ret = write(this->writefilefd, write_buf, record.length());

    if(ret < 0)
      cout<<"[filebridge]Writing to write file failed"<<endl;
  }

  // release file lock
  fcntl(this->writefilefd, F_SETLK, &ulock);
}
*/

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


//int filebridge::get_writeid()
//{
//  return writeid;
//}

//void filebridge::set_writeid(int num)
//{
//  writeid = num;
//}

#endif
