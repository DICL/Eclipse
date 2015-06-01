#ifndef __FILECLIENT__
#define __FILECLIENT__

#include <iostream>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include "msgaggregator.hh"
#include "ecfs.hh" 

using namespace std;

class fileclient   // each task process will have two objects(read/write) of fileclient
{
    private:
        int serverfd;
        char* token;
        char* ptr;
        datatype readdatatype;
        string currentkey;
        string Owritepath;
        char read_buf[BUF_SIZE];
        char write_buf[BUF_SIZE];
        
    public:
        msgaggregator Iwritebuffer; // a buffer for the Iwrite
        msgaggregator Owritebuffer; // a buffer for the Owrite
        
        fileclient();
        ~fileclient();
        bool write_record (string filename, string data, datatype atype);   // append mode, write a record
        void close_server(); // this function is used to notify the server that writing is done
        void wait_write (set<int>* peerids);   // wait until write is done
        bool read_request (string req, datatype atype);   // connect to read file
        bool read_record (string& record);   // read sentences from connected file(after read_request())
        int connect_to_server(); // returns fd of file server
        void configure_buffer_initial (string jobdirpath, string appname, string inputfilepath, bool isIcache);   // set the initial string of the Iwritebuffer
};

#endif
