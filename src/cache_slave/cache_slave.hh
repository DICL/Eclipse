// vim : fm=marker 
#ifndef __CACHE_SLAVE_HH_
#define __CACHE_SLAVE_HH_

#include <vector>

#include "../common/ecfs.hh"
#include "../common/histogram.hh"
#include "../common/dataentry.hh"
#include "../common/logger.hh"
#include "file_connclient.hh"
#include "cache.hh"
#include "messagebuffer.hh"
#include "filepeer.hh"
#include "filebridge.hh"
#include "idistributor.hh"
#include "ireader.hh"
#include "iwriter.hh"
#include "writecount.hh"

using namespace std;

class Cache_slave {
    private:
        int serverfd, cacheserverfd, ipcfd;
        int fbidclock;
        int networkidx;
        int dhtport;
        Logger* log;

        Settings setted;
        string localhostname, master_address;
        string scratch_path, ipc_path;
        vector<string> nodelist;

        histogram* thehistogram;
        cache* thecache;
        vector<file_connclient*> clients;
        vector<filebridge*> bridges;
        vector<iwriter*> iwriters;
        vector<ireader*> ireaders;
        vector<filepeer*> peers;
        
        char read_buf[BUF_SIZE];
        char write_buf[BUF_SIZE];

        const int buffersize = 8388608; // 8 MB buffer size

        filepeer* find_peer (string&);
        filebridge* find_bridge (int);
        filebridge* find_Icachebridge (string, int&);
        bool write_file (string, string&);
        
    public:
        Cache_slave();
        ~Cache_slave() {}
        int run_server ();
        int connect ();
};

#endif
