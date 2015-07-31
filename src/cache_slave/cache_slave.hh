#ifndef __CACHE_SLAVE_HH_
#define __CACHE_SLAVE_HH_
// vim : fm=marker 

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <common/ecfs.hh>
#include <common/histogram.hh>
#include <common/dataentry.hh>
#include "file_connclient.hh"
#include "cache.hh"
#include "messagebuffer.hh"
#include "filepeer.hh"
#include "filebridge.hh"
#include "idistributor.hh"
#include "ireader.hh"
#include "iwriter.hh"
#include "writecount.hh"
#include <sys/fcntl.h>

using namespace std;

class Cache_slave   // each slave node has an object of Cache_slave
{
    private:
        int serverfd;
        int cacheserverfd;
        int ipcfd;
        int fbidclock;
        int networkidx;
        string localhostname;
        histogram* thehistogram;
        cache* thecache;
        vector<string> nodelist;
        vector<file_connclient*> clients;
        vector<filebridge*> bridges;
        vector<iwriter*> iwriters;
        vector<ireader*> ireaders;
        
        char read_buf[BUF_SIZE];
        char write_buf[BUF_SIZE];
        string dht_path;
        
    public:
        vector<filepeer*> peers;
        
        Cache_slave();
        filepeer* find_peer (string& address);
        filebridge* find_bridge (int id);
        int run_server (int port, string master_address);
        bool write_file (string fname, string& record);
        filebridge* find_Icachebridge (string inputname, int& bridgeindex);
};

#endif
