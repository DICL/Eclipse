#ifndef __FILE_CONNCLIENT__
#define __FILE_CONNCLIENT__

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include "settings.hh"
#include "ecfs.hh"

using namespace std;
class file_connclient
{
    private:
        int fd;
        int writefilefd;
        fstream readfilestream;
        string filename;
        file_role role;
        
    public:
        file_connclient (int fd);
        file_connclient (int fd, file_role, string aname);
        ~file_connclient();
        
        int get_fd();
        void set_role (file_role);
        file_role get_role();
        void set_filename (string name);
        string get_filename();
        void open_readfile (string fname);
        void open_writefile (string fname);
        bool read_record (string* record);
        void write_record (string record, char* write_buf);
};
#endif
