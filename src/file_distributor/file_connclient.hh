#ifndef __FILE_CONNCLIENT__
#define __FILE_CONNCLIENT__

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <orthrus/dataentry.hh>
#include <file_distributor/filepeer.hh>
#include "messagebuffer.hh"
#include "idistributor.hh"
#include "writecount.hh"
#include <common/ecfs.hh>

using namespace std;

class file_connclient
{
    private:
        int fd;
        int dstid;
        string Icachekey;
        entrywriter* Icachewriter; // cache writer for the intermediate results
        filepeer* Itargetpeer;
        //file_role role;
        
    public:
        idistributor* thedistributor;
        writecount* thecount;
        vector<messagebuffer*> msgbuf;
        
        file_connclient (int number);
        //file_connclient(int fd, file_role arole);
        ~file_connclient();
        
        int get_fd();
        //int get_writeid();
        void set_fd (int num);
        entrywriter* get_Icachewriter();
        void set_Icachewriter (entrywriter* thewriter);
        filepeer* get_Itargetpeer();
        void set_Itargetpeer (filepeer* thepeer);
        int get_dstid();
        void set_dstid (int anumber);
        void set_Icachekey (string key);
        string get_Icachekey();
        //void set_writeid(int num);
        //void set_role(file_role arole);
        //file_role get_role();
};

file_connclient::file_connclient (int number)
{
    fd = number;
    thecount = NULL;
    Icachewriter = NULL;
    Itargetpeer = NULL;
    thedistributor = NULL;
    dstid = -1;
    //this->writeid = -1;
    //this->role = UNDEFINED;
    // add a null buffer
    msgbuf.push_back (new messagebuffer());
}

//file_connclient::file_connclient(int fd, file_role arole)
//{
//  this->fd = fd;
//  //this->role = arole;
//  this->writeid = -1;
//
//  // add a null buffer
//  msgbuf.push_back(new messagebuffer());
//}

file_connclient::~file_connclient()
{
    for (int i = 0; (unsigned) i < msgbuf.size(); i++)
    {
        delete msgbuf[i];
    }
    
    if (thecount != NULL)
    {
        delete thecount;
    }
    
    if (Icachewriter != NULL)
    {
        Icachewriter->complete();
        delete Icachewriter;
    }
    
    if (thedistributor != NULL)
    {
        delete thedistributor;
    }
}

int file_connclient::get_fd()
{
    return this->fd;
}

void file_connclient::set_fd (int num)
{
    this->fd = num;
}

entrywriter* file_connclient::get_Icachewriter()
{
    return Icachewriter;
}
void file_connclient::set_Icachewriter (entrywriter* thewriter)
{
    Icachewriter = thewriter;
}

filepeer* file_connclient::get_Itargetpeer()
{
    return Itargetpeer;
}

void file_connclient::set_Itargetpeer (filepeer* thepeer)
{
    Itargetpeer = thepeer;
}

int file_connclient::get_dstid()
{
    return dstid;
}

void file_connclient::set_dstid (int anumber)
{
    dstid = anumber;
}

void file_connclient::set_Icachekey (string key)
{
    Icachekey = key;
}

string file_connclient::get_Icachekey()
{
    return Icachekey;
}

//void file_connclient::set_role(file_role arole)
//{
//  this->role = arole;
//}

//file_role file_connclient::get_role()
//{
//  return this->role;
//}

//int file_connclient::get_writeid()
//{
//  return writeid;
//}

//void file_connclient::set_writeid(int num)
//{
//  writeid = num;
//}

#endif
