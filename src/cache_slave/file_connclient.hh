#ifndef __FILE_CONNCLIENT__
#define __FILE_CONNCLIENT__

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <common/dataentry.hh>
#include <common/ecfs.hh>
#include "filepeer.hh"
#include "messagebuffer.hh"
#include "idistributor.hh"
#include "writecount.hh"

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

#endif
