#ifndef __FILESERVER__
#define __FILESERVER__

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <common/ecfs.hh>
#include <common/hash.hh>
#include <common/msgaggregator.hh>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "file_connclient.hh"
#include <orthrus/histogram.hh>
#include <orthrus/cache.hh>
#include <orthrus/dataentry.hh>
#include "messagebuffer.hh"
#include "filepeer.hh"
#include "filebridge.hh"
#include "idistributor.hh"
#include "ireader.hh"
#include "iwriter.hh"
#include "writecount.hh"
#include <sys/fcntl.h>
#include "../common/settings.hh"

using namespace std;

class fileserver   // each slave node has an object of fileserver
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
        
        fileserver();
        filepeer* find_peer (string& address);
        filebridge* find_bridge (int id);
        int run_server (int port, string master_address);
        bool write_file (string fname, string& record);
        filebridge* find_Icachebridge (string inputname, int& bridgeindex);
};

fileserver::fileserver()
{
    networkidx = -1;
    serverfd = -1;
    cacheserverfd = -1;
    ipcfd = -1;
    fbidclock = 0; // fb id starts from 0

    Settings setted;
    setted.load_settings();
    dht_path = setted.scratch_path();
}

int fileserver::run_server (int port, string master_address)
{
    int buffersize = 8388608; // 8 MB buffer size
    // read hostname from hostname file
    ifstream hostfile;
    string word;
    
    Settings setted;
    setted.load_settings();
    string hostpath = setted.scratch_path();
    hostpath.append ("/hostname");
    hostfile.open (hostpath.c_str());
    hostfile >> localhostname;
    hostfile.close();

    nodelist = setted.nodelist();
    string ipc_path = setted.ipc_path();
    
    if (access (ipc_path.c_str(), F_OK) == 0)
    {
        unlink (ipc_path.c_str());
    }
    
    // determine the network topology by reading node list information
    for (int i = 0; (unsigned) i < nodelist.size(); i++)
    {
        if (nodelist[i] == localhostname)
        {
            networkidx = i;
            break;
        }
    }
    
    // connect to cacheserver
    {
        cacheserverfd = -1;
        struct sockaddr_in serveraddr;
        struct hostent *hp;
        // SOCK_STREAM -> tcp
        cacheserverfd = socket (AF_INET, SOCK_STREAM, 0);
        
        if (cacheserverfd < 0)
        {
            cout << "[fileserver:" << networkidx << "]Openning socket failed" << endl;
            exit (1);
        }
        
        hp = gethostbyname (master_address.c_str());
        memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
        serveraddr.sin_family = AF_INET;
        memcpy (&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
        serveraddr.sin_port = htons (port);
        
        while (connect (cacheserverfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0)
        {
            cout << "[fileserver:" << networkidx << "]Cannot connect to the cache server. Retrying..." << endl;
            //cout<<"\thost name: "<<nodelist[i]<<endl;
            usleep (100000);
            continue;
        }
        
        fcntl (cacheserverfd, F_SETFL, O_NONBLOCK);
        setsockopt (cacheserverfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
        setsockopt (cacheserverfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
    }
    
    // connect to other peer eclipse nodes
    for (int i = 0; i < networkidx; i++)
    {
        int clientfd = -1;
        struct sockaddr_in serveraddr;
        struct hostent *hp = NULL;
        // SOCK_STREAM -> tcp
        clientfd = socket (AF_INET, SOCK_STREAM, 0);
        
        if (clientfd < 0)
        {
            cout << "[fileserver:" << networkidx << "]Openning socket failed" << endl;
            exit (1);
        }
        
        hp = gethostbyname (nodelist[i].c_str());
        
        if (hp == NULL)
        {
            cout << "[fileserver:" << networkidx << "]Cannot find host by host name" << endl;
            return -1;
        }
        
        memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
        serveraddr.sin_family = AF_INET;
        memcpy (&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
        serveraddr.sin_port = htons (port);
        
        while (connect (clientfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0)
        {
            cout << "[fileserver:" << networkidx << "]Cannot connect to: " << nodelist[i] << endl;
            cout << "Retrying..." << endl;
            usleep (100000);
            //cout<<"\thost name: "<<nodelist[i]<<endl;
            continue;
        }
        
        // register the file peer
        peers.push_back (new filepeer (clientfd, nodelist[i]));
        // set the peer fd as nonblocking mode
        fcntl (clientfd, F_SETFL, O_NONBLOCK);
        setsockopt (clientfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
        setsockopt (clientfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
    }
    
    // socket open for listen
    int fd;
    int tmpfd = -1;
    struct sockaddr_in serveraddr;
    fd = socket (AF_INET, SOCK_STREAM, 0);
    
    if (fd < 0)
    {
        cout << "[fileserver:" << networkidx << "]Socket opening failed" << endl;
        exit (-1);
    }
    
    int valid = 1;
    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof (valid));
    // bind
    memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);
    serveraddr.sin_port = htons ( (unsigned short) port);
    
    if (bind (fd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0)
    {
        cout << "[fileserver:" << networkidx << "]\033[0;31mBinding failed\033[0m" << endl;
        exit (-1);
    }
    
    // listen
    if (listen (fd, BACKLOG) < 0)
    {
        cout << "[master]Listening failed" << endl;
        exit (-1);
        return -1;
    }
    
    // register the current node itself to the peer list
    peers.push_back (new filepeer (-1, localhostname));
    
    // register the other peers in order
    for (int i = networkidx + 1; (unsigned) i < nodelist.size(); i++)
    {
        peers.push_back (new filepeer (-1, nodelist[i]));
    }
    
    // listen connections from peers and complete the eclipse network
    for (int i = networkidx + 1; (unsigned) i < nodelist.size(); i++)
    {
        struct sockaddr_in connaddr;
        int addrlen = sizeof (connaddr);
        tmpfd = accept (fd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
        
        if (tmpfd > 0)
        {
            char* haddrp = inet_ntoa (connaddr.sin_addr);
            string address = haddrp;
            
            for (int j = networkidx + 1; (unsigned) j < nodelist.size(); j++)
            {
                if (peers[j]->get_address() == address)
                {
                    peers[j]->set_fd (tmpfd);
                }
            }
            
            // set the peer fd as nonblocking mode
            fcntl (tmpfd, F_SETFL, O_NONBLOCK);
            setsockopt (tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
            setsockopt (tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
        }
        else if (tmpfd < 0)       // connection failed
        {
            // retry with same index
            i--;
            continue;
        }
        else
        {
            cout << "connection closed......................." << endl;
        }
    }
    
    // register the server fd
    serverfd = fd;
    cout << "[fileserver:" << networkidx << "]Eclipse network successfully established(id=" << networkidx << ")" << endl;
    // prepare AF_UNIX socket for the ipc with tasks
    struct sockaddr_un serveraddr2;
    ipcfd = socket (AF_UNIX, SOCK_STREAM, 0);
    
    if (ipcfd < 0)
    {
        cout << "[fileserver:" << networkidx << "]AF_UNIX socket openning failed" << endl;
        exit (-1);
    }
    
    // bind
    memset ( (void*) &serveraddr2, 0, sizeof (serveraddr2));
    serveraddr2.sun_family = AF_UNIX;
    strcpy (serveraddr2.sun_path, ipc_path.c_str());
    
    if (bind (ipcfd, (struct sockaddr *) &serveraddr2, SUN_LEN (&serveraddr2)) < 0)
    {
        cout << "[fileserver:" << networkidx << "]\033[0;31mIPC Binding failed\033[0m" << endl;
        exit (-1);
    }
    
    // listen
    if (listen (ipcfd, BACKLOG) < 0)
    {
        cout << "[master]Listening failed" << endl;
        exit (-1);
    }
    
    // set the server fd and ipc fd as nonblocking mode
    fcntl (ipcfd, F_SETFL, O_NONBLOCK);
    fcntl (serverfd, F_SETFL, O_NONBLOCK);
    setsockopt (ipcfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
    setsockopt (ipcfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
    setsockopt (serverfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
    setsockopt (serverfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
    tmpfd = -1;
    // initialize the histogram
    thehistogram = new histogram (nodelist.size(), NUMBIN);
    // initialize the local cache
    thecache = new cache (CACHESIZE);
    struct timeval time_start;
    struct timeval time_end;
    gettimeofday (&time_start, NULL);
    gettimeofday (&time_end, NULL);
    
    // start main loop which listen to connections and signals from clients and peers
    while (1)
    {
//gettimeofday(&time_start2, NULL);
        // local clients
        tmpfd = accept (ipcfd, NULL, NULL);
        
        if (tmpfd > 0)     // new file client is connected
        {
            // create new clients
            clients.push_back (new file_connclient (tmpfd));
            clients.back()->thecount = new writecount();
            // set socket to be non-blocking socket to avoid deadlock
            fcntl (tmpfd, F_SETFL, O_NONBLOCK);
            setsockopt (tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
            setsockopt (tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
        }
        
//gettimeofday(&time_end2, NULL);
//timeslot1 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);
        for (int i = 0; (unsigned) i < clients.size(); i++)
        {
            int readbytes = -1;
            readbytes = nbread (clients[i]->get_fd(), read_buf);
            
            if (readbytes > 0)
            {
                char* token;
                string filename;
                
//        memset(write_buf, 0, BUF_SIZE);
//        strcpy(write_buf, read_buf);
                // The message is either: Rread(raw), Iread(intermediate), Iwrite(intermediate), Owrite(output)
                if (strncmp (read_buf, "Rread", 5) == 0)
                {
                    string cachekey;
                    string appname;
                    int jobid;
                    token = strtok (read_buf, " ");   // <- "Rread"
                    // determine the candidate eclipse node which will have the data
                    string address;
                    token = strtok (NULL, " ");   // <- [app name]
                    appname = token; // [app name]
                    cachekey = token; // [app name]
                    cachekey.append ("_");
                    token = strtok (NULL, " ");   // <- job id
                    jobid = atoi (token);
                    token = strtok (NULL, " ");   // <- file name
                    cachekey.append (token);   // <- cachekey: [app name]_[file name]
                    filename = token;
                    // determine the cache location of data
                    memset (read_buf, 0, HASHLENGTH);
                    strcpy (read_buf, filename.c_str());
                    int index;
                    uint32_t hashvalue = h (read_buf, HASHLENGTH);
                    index = thehistogram->get_index (hashvalue);
                    address = nodelist[index];
                    filebridge* thebridge = new filebridge (fbidclock++);
                    bridges.push_back (thebridge);
                    
                    if (index == networkidx)     // local input data
                    {
                        // check whether the intermediate result is hit
                        dataentry* theentry = thecache->lookup (cachekey);
                        
                        if (theentry == NULL)     // when the intermediate result is not hit
                        {
                            cout << "\033[0;31mLocal Icache miss\033[0m" << endl;
                            // send 0 packet to the client to notify that Icache is not hit
                            memset (write_buf, 0, BUF_CUT);
                            
                            if (clients[i]->msgbuf.size() > 1)
                            {
                                clients[i]->msgbuf.back()->set_buffer (write_buf, clients[i]->get_fd());
                                clients[i]->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (clients[i]->get_fd(), write_buf, clients[i]->msgbuf.back()) <= 0)
                                {
                                    clients[i]->msgbuf.push_back (new messagebuffer());
                                }
                            }
                            
                            theentry = thecache->lookup (filename);
                            
                            if (theentry == NULL)     // when data is not in cache
                            {
                                cout << "\033[0;31mLocal Cache miss\033[0m" << endl;
                                // set an entry writer
                                dataentry* newentry = new dataentry (filename, hashvalue);
                                thecache->new_entry (newentry);
                                entrywriter* thewriter = new entrywriter (newentry);
                                // determine the DHT file location
                                hashvalue = hashvalue % nodelist.size();
                                address = nodelist[hashvalue];
                                
                                if ( (unsigned) networkidx == hashvalue)
                                {
                                    // 1. read data from disk
                                    // 2. store it in the cache
                                    // 3. send it to client
                                    thebridge->set_srctype (DISK);
                                    thebridge->set_dsttype (CLIENT);
                                    thebridge->set_entrywriter (thewriter);
                                    thebridge->set_dstclient (clients[i]);
                                    thebridge->writebuffer = new msgaggregator (clients[i]->get_fd());
                                    thebridge->writebuffer->configure_initial ("");
                                    thebridge->writebuffer->set_msgbuf (&clients[i]->msgbuf);
                                    thebridge->writebuffer->set_dwriter (thewriter);
                                    //thebridge->set_filename(filename);
                                    //thebridge->set_dataname(filename);
                                    //thebridge->set_dtype(RAW);
                                    // open read file and start sending data to client
                                    thebridge->open_readfile (filename);
                                }
                                else     // remote DHT peer
                                {
                                    // 1. request to the DHT peer (read from peer)
                                    // 2. store it in the cache
                                    // 3. send it to client
                                    thebridge->set_srctype (PEER);
                                    thebridge->set_dsttype (CLIENT);
                                    thebridge->set_entrywriter (thewriter);
                                    thebridge->set_dstclient (clients[i]);
                                    // thebridge->set_filename(filename);
                                    // thebridge->set_dataname(filename);
                                    // thebridge->set_dtype(RAW);
                                    // send message to the target peer
                                    string message;
                                    stringstream ss;
                                    ss << "RDread ";
                                    ss << filename;
                                    ss << " ";
                                    ss << thebridge->get_id();
                                    message = ss.str();
                                    memset (write_buf, 0, BUF_SIZE);
                                    strcpy (write_buf, message.c_str());
                                    //cout<<endl;
                                    //cout<<"write from: "<<localhostname<<endl;
                                    //cout<<"write to: "<<address<<endl;
                                    //cout<<"message: "<<write_buf<<endl;
                                    //cout<<endl;
                                    filepeer* thepeer = find_peer (address);
                                    
                                    if (thepeer->msgbuf.size() > 1)
                                    {
                                        thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                        thepeer->msgbuf.push_back (new messagebuffer());
                                    }
                                    else
                                    {
                                        if (nbwritebuf (thepeer->get_fd(),
                                                        write_buf, thepeer->msgbuf.back()) <= 0)
                                        {
                                            thepeer->msgbuf.push_back (new messagebuffer());
                                        }
                                    }
                                }
                            }
                            else     // when the data is in the cache
                            {
                                if (theentry->is_being_written())
                                {
                                    // determine the DHT file location
                                    hashvalue = hashvalue % nodelist.size();
                                    address = nodelist[hashvalue];
                                    
                                    if ( (unsigned) networkidx == hashvalue)
                                    {
                                        // 1. read data from disk
                                        // 2. store it in the cache
                                        // 3. send it to client
                                        thebridge->set_srctype (DISK);
                                        thebridge->set_dsttype (CLIENT);
                                        thebridge->set_dstclient (clients[i]);
                                        thebridge->writebuffer = new msgaggregator (clients[i]->get_fd());
                                        thebridge->writebuffer->configure_initial ("");
                                        thebridge->writebuffer->set_msgbuf (&clients[i]->msgbuf);
                                        //thebridge->set_filename(filename);
                                        //thebridge->set_dataname(filename);
                                        //thebridge->set_dtype(RAW);
                                        // open read file and start sending data to client
                                        thebridge->open_readfile (filename);
                                    }
                                    else     // remote DHT peer
                                    {
                                        // 1. request to the DHT peer (read from peer)
                                        // 2. store it in the cache
                                        // 3. send it to client
                                        thebridge->set_srctype (PEER);
                                        thebridge->set_dsttype (CLIENT);
                                        thebridge->set_dstclient (clients[i]);
                                        // thebridge->set_filename(filename);
                                        // thebridge->set_dataname(filename);
                                        // thebridge->set_dtype(RAW);
                                        // send message to the target peer
                                        string message;
                                        stringstream ss;
                                        ss << "RDread ";
                                        ss << filename;
                                        ss << " ";
                                        ss << thebridge->get_id();
                                        message = ss.str();
                                        memset (write_buf, 0, BUF_SIZE);
                                        strcpy (write_buf, message.c_str());
                                        //cout<<endl;
                                        //cout<<"write from: "<<localhostname<<endl;
                                        //cout<<"write to: "<<address<<endl;
                                        //cout<<"message: "<<write_buf<<endl;
                                        //cout<<endl;
                                        filepeer* thepeer = find_peer (address);
                                        
                                        if (thepeer->msgbuf.size() > 1)
                                        {
                                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                            thepeer->msgbuf.push_back (new messagebuffer());
                                        }
                                        else
                                        {
                                            if (nbwritebuf (thepeer->get_fd(),
                                                            write_buf, thepeer->msgbuf.back()) <= 0)
                                            {
                                                thepeer->msgbuf.push_back (new messagebuffer());
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    // 1. read from cache
                                    // 2. and send it to client
                                    // set a entry reader
                                    entryreader* thereader = new entryreader (theentry);
                                    cout << "\033[0;32m[" << networkidx << "]Local Cache hit\033[0m" << endl;
                                    thebridge->set_srctype (CACHE);
                                    thebridge->set_dsttype (CLIENT);
                                    thebridge->set_entryreader (thereader);
                                    thebridge->set_dstclient (clients[i]);
                                    //thebridge->set_filename(filename);
                                    //thebridge->set_dataname(filename);
                                    //thebridge->set_dtype(RAW);
                                }
                            }
                        }
                        else     // intermediate result hit
                        {
                            cout << "\033[0;32m[" << networkidx << "]Local Icache hit\033[0m" << endl;
//cout<<"[fileserver:"<<networkidx<<"]Cachekey hit: "<<cachekey<<endl;
                            // send 1 packet to the client to notify that Icache is hit
                            memset (write_buf, 0, BUF_CUT);
                            write_buf[0] = 1;
                            
                            if (clients[i]->msgbuf.size() > 1)
                            {
                                clients[i]->msgbuf.back()->set_buffer (write_buf, clients[i]->get_fd());
                                clients[i]->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (clients[i]->get_fd(), write_buf, clients[i]->msgbuf.back()) <= 0)
                                {
                                    clients[i]->msgbuf.push_back (new messagebuffer());
                                }
                            }
                            
                            // set an entry reader and idistributor
                            entryreader* thereader = new entryreader (theentry);
                            idistributor* thedistributor = new idistributor ( (&peers), (&iwriters), clients[i]->thecount, jobid, networkidx);
                            thebridge->set_srctype (CACHE);
                            thebridge->set_dsttype (DISTRIBUTE);
                            thebridge->set_dstclient (clients[i]);   // to notify the end of distribution
                            thebridge->set_distributor (thedistributor);
                            thebridge->set_entryreader (thereader);
                        }
                        
                        /*
                        // check whether the input is hit
                        dataentry* theentry = thecache->lookup(filename);
                        
                        if(theentry == NULL) // when data is not in cache
                        {
                          cout<<"\033[0;31mCache miss\033[0m"<<endl;
                        
                          // set a entry writer
                          dataentry* newentry = new dataentry(filename, hashvalue);
                          thecache->new_entry(newentry);
                          entrywriter* thewriter = new entrywriter(newentry);
                        
                          // determine the DHT file location
                          hashvalue = hashvalue%nodelist.size();
                        
                          address = nodelist[hashvalue];
                        
                          if(localhostname == address)
                          {
                            // 1. read data from disk
                            // 2. store it in the cache
                            // 3. send it to client
                        
                            thebridge->set_srctype(DISK);
                            thebridge->set_dsttype(CLIENT);
                            thebridge->set_entrywriter(thewriter);
                            thebridge->set_dstclient(clients[i]);
                        
                            thebridge->writebuffer = new msgaggregator(clients[i]->get_fd());
                            thebridge->writebuffer->configure_initial("");
                            thebridge->writebuffer->set_msgbuf(&clients[i]->msgbuf);
                            thebridge->writebuffer->set_dwriter(thewriter);
                            //thebridge->set_filename(filename);
                            //thebridge->set_dataname(filename);
                            //thebridge->set_dtype(RAW);
                        
                            // open read file and start sending data to client
                            thebridge->open_readfile(filename);
                          }
                          else // remote DHT peer
                          {
                            // 1. request to the DHT peer (read from peer)
                            // 2. store it in the cache
                            // 3. send it to client
                        
                            thebridge->set_srctype(PEER);
                            thebridge->set_dsttype(CLIENT);
                            thebridge->set_entrywriter(thewriter);
                            thebridge->set_dstclient(clients[i]);
                        
                            // thebridge->set_filename(filename);
                            // thebridge->set_dataname(filename);
                            // thebridge->set_dtype(RAW);
                        
                            // send message to the target peer
                            string message;
                            stringstream ss;
                            ss << "RDread ";
                            ss << filename;
                            ss << " ";
                            ss << thebridge->get_id();
                            message = ss.str();
                        
                            memset(write_buf, 0, BUF_SIZE);
                            strcpy(write_buf, message.c_str());
                        
                            //cout<<endl;
                            //cout<<"write from: "<<localhostname<<endl;
                            //cout<<"write to: "<<address<<endl;
                            //cout<<"message: "<<write_buf<<endl;
                            //cout<<endl;
                        
                            filepeer* thepeer = find_peer(address);
                        
                            if(thepeer->msgbuf.size() > 1)
                            {
                              thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
                              thepeer->msgbuf.push_back(new messagebuffer());
                            }
                            else
                            {
                              if(nbwritebuf(thepeer->get_fd(),
                                    write_buf, thepeer->msgbuf.back()) <= 0)
                              {
                                thepeer->msgbuf.push_back(new messagebuffer());
                              }
                            }
                          }
                        }
                        else // when the data is in the cache
                        {
                          // 1. read from cache
                          // 2. and send it to client
                        
                          // set a entry reader
                          entryreader* thereader = new entryreader(theentry);
                        
                          cout<<"\033[0;32mCache hit\033[0m"<<endl;
                        
                          thebridge->set_srctype(CACHE);
                          thebridge->set_dsttype(CLIENT);
                          thebridge->set_entryreader(thereader);
                          thebridge->set_dstclient(clients[i]);
                          //thebridge->set_filename(filename);
                          //thebridge->set_dataname(filename);
                          //thebridge->set_dtype(RAW);
                        }
                        */
                    }
                    else     // remote cache peer
                    {
                        // 1. request to the remote cache peer
                        // 2. send it to client
                        thebridge->set_srctype (PEER);
                        thebridge->set_dsttype (CLIENT);
                        thebridge->set_dstclient (clients[i]);
                        //thebridge->set_filename(filename);
                        //thebridge->set_dataname(filename);
                        //thebridge->set_dtype(RAW);
                        // send message to the target peer
                        string message;
                        stringstream ss;
                        ss << "Ilook ";
                        ss << appname;
                        ss << " ";
                        ss << filename;
                        ss << " ";
                        ss << jobid;
                        ss << " ";
                        ss << thebridge->get_id();
                        message = ss.str();
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        //cout<<endl;
                        //cout<<"write from: "<<localhostname<<endl;
                        //cout<<"write to: "<<address<<endl;
                        //cout<<"message: "<<write_buf<<endl;
                        //cout<<endl;
                        filepeer* thepeer = find_peer (address);
                        
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                        }
                    }
                }
                else if (strncmp (read_buf, "Iread", 5) == 0)
                {
//cout<<"[fileserver:"<<networkidx<<"]Iread message: "<<read_buf<<endl;
                    int jobid;
                    int peerid;
                    int numiblock;
                    token = strtok (read_buf, " \n");   // <- "Iread"
                    token = strtok (NULL, " ");   // <- [job id]
                    jobid = atoi (token);
                    token = strtok (NULL, " ");   // <- [peer id]
                    peerid = atoi (token);
                    token = strtok (NULL, " ");   // <- numiblock
                    numiblock = atoi (token);
                    
                    if (peerid == networkidx)     // local
                    {
                        // generate ireader and link to the client
                        ireader* thereader = new ireader (jobid, numiblock, peerid, 0, CLIENT);
                        thereader->set_dstclient (clients[i]);
                        ireaders.push_back (thereader);
                    }
                    else     // remote
                    {
                        // send iwriter request to the target peer
                        filebridge* thebridge = new filebridge (fbidclock++);
                        bridges.push_back (thebridge);
                        thebridge->set_srctype (PEER);
                        thebridge->set_dsttype (CLIENT);
                        thebridge->set_dstclient (clients[i]);
                        //thebridge->set_filename(filename);
                        //thebridge->set_dataname(filename);
                        //thebridge->set_dtype(INTERMEDIATE);
                        // send message to the target peer
                        string message;
                        stringstream ss;
                        ss << "Iread ";
                        ss << jobid;
                        ss << " ";
                        ss << numiblock;
                        ss << " ";
                        ss << thebridge->get_id();
                        message = ss.str();
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        //cout<<endl;
                        //cout<<"write from: "<<localhostname<<endl;
                        //cout<<"write to: "<<address<<endl;
                        //cout<<"message: "<<write_buf<<endl;
                        //cout<<endl;
                        filepeer* thepeer = peers[peerid];
                        
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            //cout<<endl;
                            //cout<<"from: "<<localhostname<<endl;
                            //cout<<"to: "<<thepeer->get_address()<<endl;
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                        }
                        
                        //nbwrite(find_peer(address)->get_fd(), write_buf);
                    }
                }
                else if (strncmp (read_buf, "ICwrite", 6) == 0)         // "ICwrite" message from client
                {
                    string cachekey;
                    string appname;
                    string inputpath;
                    int jobid;
                    token = strtok (read_buf, " ");   // <- "ICwrite"
                    token = strtok (NULL, "_");   // <- ".job"
                    token = strtok (NULL, "_\n");   // <- job id
                    jobid = atoi (token);
                    token = strtok (NULL, " ");   // <- [app name]
                    appname = token; // <- [app name]
                    cachekey = token; // <- [app name]
                    cachekey.append ("_");
                    token = strtok (NULL, "\n");   // <- [input file path]
                    inputpath = token;
                    cachekey.append (inputpath);
                    token = strtok (NULL, "");   // token -> rest of message
//cout<<"ICwrite sent"<<endl;
//cout<<"cachekey: "<<cachekey<<endl;
//cout<<"inputpath: "<<inputpath<<endl;
//cout<<"jobdirpath: "<<jobdirpath<<endl;
                    // cache the 'token'
                    entrywriter* thewriter = clients[i]->get_Icachewriter();
                    filepeer* thepeer = clients[i]->get_Itargetpeer();
                    
                    if (thewriter != NULL)     // target is local and writing is ongoing
                    {
//cout<<"[fileserver:"<<networkidx<<"]Cachekey of written data: "<<cachekey<<endl;
                        thewriter->write_record (token);
                    }
                    else if (thepeer != NULL)       // target is remote peer and writing is ongoing
                    {
                        string message = "Icache ";
                        message.append (appname);
                        message.append (" ");
                        message.append (inputpath);
                        message.append ("\n");
                        message.append (token);
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        
//cout<<"[fileserver:"<<networkidx<<"]Sending Icache message: "<<write_buf<<endl;
                        // send token to the remote cache to store it
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                        }
                    }
                    else     // target should be determined
                    {
                        // determine the target cache of data
                        string address;
                        memset (write_buf, 0, HASHLENGTH);
                        strcpy (write_buf, inputpath.c_str());
                        int index;
                        uint32_t hashvalue = h (write_buf, HASHLENGTH);
                        index = thehistogram->get_index (hashvalue);
                        address = nodelist[index];
                        
                        if (index == networkidx)     // local target cache
                        {
                            // set a entry writer
//cout<<"[fileserver:"<<networkidx<<"]Cachekey of new data entry: "<<cachekey<<endl;
                            dataentry* newentry = new dataentry (cachekey, hashvalue);
                            thecache->new_entry (newentry);
                            entrywriter* thewriter = new entrywriter (newentry);
                            clients[i]->set_Icachewriter (thewriter);
                            thewriter->write_record (token);
                        }
                        else     // remote peer cache
                        {
                            thepeer = peers[index];
                            clients[i]->set_Itargetpeer (thepeer);
                            clients[i]->set_Icachekey (cachekey);
                            string message = "Icache ";
                            message.append (appname);
                            message.append (" ");
                            message.append (inputpath);
                            message.append ("\n");
                            message.append (token);
                            memset (write_buf, 0, BUF_SIZE);
                            strcpy (write_buf, message.c_str());
                            
//cout<<"[fileserver:"<<networkidx<<"]Sending Icache message: "<<write_buf<<endl;
                            // send token to the remote cache to store it
                            if (thepeer->msgbuf.size() > 1)
                            {
                                thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (thepeer->get_fd(),
                                                write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                    thepeer->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                    }
                    
                    // find or allocate idistributor
                    if (clients[i]->thedistributor == NULL)
                    {
                        clients[i]->thedistributor = new idistributor ( (&peers), (&iwriters), clients[i]->thecount, jobid, networkidx);
                        clients[i]->thedistributor->process_message (token, write_buf);
                    }
                    else
                    {
                        // process the intermediate data
                        clients[i]->thedistributor->process_message (token, write_buf);
                    }
                    
                    /*
                    if(localhostname == address)
                    {
                      // open write file to write data to disk
                      write_file(filename, record);
                      //filebridge* thebridge = new filebridge(fbidclock++);
                      //thebridge->open_writefile(filename);
                      //thebridge->write_record(record, write_buf);
                    
                      //delete thebridge;
                    }
                    else // distant
                    {
                      // bridges.push_back(thebridge);
                    
                      // set up the bridge
                      // thebridge->set_srctype(PEER); // source of Ewrite
                      // thebridge->set_dsttype(CLIENT); // destination of Ewrite
                      // thebridge->set_dstclient(NULL);
                      // thebridge->set_dtype(INTERMEDIATE);
                      // thebridge->set_writeid(writeid);
                    
                      // send message along with the record to the target peer
                      string message;
                      stringstream ss;
                      ss << "write ";
                      ss << filename;
                      ss << " ";
                      ss << record;
                      message = ss.str();
                    
                      memset(write_buf, 0, BUF_SIZE);
                      strcpy(write_buf, message.c_str());
                    
                      //cout<<endl;
                      //cout<<"write from: "<<localhostname<<endl;
                      //cout<<"write to: "<<address<<endl;
                      //cout<<"message: "<<write_buf<<endl;
                      //cout<<endl;
                    
                      filepeer* thepeer = peers[hashvalue];
                    
                      if(clients[i]->thecount == NULL)
                        cout<<"[fileserver]The write id is not set before the write request"<<endl;
                    
                      clients[i]->thecount->add_peer(hashvalue);
                    
                      if(thepeer->msgbuf.size() > 1)
                      {
                        thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
                        thepeer->msgbuf.push_back(new messagebuffer());
                      }
                      else
                      {
                        //cout<<endl;
                        //cout<<"from: "<<localhostname<<endl;
                        //cout<<"to: "<<thepeer->get_address()<<endl;
                        if(nbwritebuf(thepeer->get_fd(),
                              write_buf, thepeer->msgbuf.back()) <= 0)
                        {
                          thepeer->msgbuf.push_back(new messagebuffer());
                        }
                      }
                    }
                    */
                }
                else if (strncmp (read_buf, "Iwrite", 6) == 0)         // "Iwrite" message from client
                {
                    int jobid;
                    // extract job id
                    token = strtok (read_buf, " ");   // <- "Iwrite"
                    token = strtok (NULL, "_");   // <- ".job"
                    token = strtok (NULL, "_\n");   // <- job id
                    jobid = atoi (token);
                    token += strlen (token) + 2;   // jump "_" and " "
                    
                    // find or allocate idistributor
                    if (clients[i]->thedistributor == NULL)
                    {
                        clients[i]->thedistributor = new idistributor ( (&peers), (&iwriters), clients[i]->thecount, jobid, networkidx);
                        clients[i]->thedistributor->process_message (token, write_buf);
                    }
                    else
                    {
                        // process the intermediate data
                        clients[i]->thedistributor->process_message (token, write_buf);
                    }
                }
                else if (strncmp (read_buf, "Owrite", 6) == 0)
                {
                    string record;
                    // pre-copy the contents of read_buf to write_buf
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, read_buf);
                    token = strtok (read_buf, " ");   // <- "Owrite"
                    token = strtok (NULL, "\n");   // <- file name
                    filename = token;
                    token += strlen (token) + 1;   // <- token: writing contents
                    record = token;
                    // determine the target peer node
                    memset (read_buf, 0, HASHLENGTH);
                    strcpy (read_buf, filename.c_str());
                    uint32_t hashvalue = h (read_buf, HASHLENGTH);
                    hashvalue = hashvalue % nodelist.size();
                    
                    if (hashvalue == (unsigned) networkidx)       // write to local disk
                    {
                        // write to the disk
                        write_file (filename, record);
                    }
                    else     // remote peer
                    {
                        // write to the peer with pre-prepared write_buf
                        filepeer* thepeer = peers[hashvalue];
                        
                        if (clients[i]->thecount == NULL)
                        {
                            cout << "[fileserver:" << networkidx << "]The write id is not set before the write request" << endl;
                        }
                        
                        // write to the peer
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                        }
                    }
                }
                else if (strncmp (read_buf, "MWwrite", 7) == 0)
                {
//cout<<"MWwrite"<<endl;
                    // flush and clear the distributor
                    if (clients[i]->thedistributor != NULL)
                    {
                        delete clients[i]->thedistributor;
                        clients[i]->thedistributor = NULL;
                    }
                    
                    // send intermediate write peer list
                    stringstream ss;
                    string message;
                    writecount* thecount = clients[i]->thecount;
                    stringstream ss1;
                    ss1 << "Wack ";
                    ss1 << clients[i]->get_fd();
                    message = ss1.str();
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, message.c_str());
                    
                    if (thecount->size() > 0)
                    {
                        set<int>::iterator it = thecount->peerids.begin();
                        ss << *it;
                        
                        if (*it != networkidx)
                        {
                            if (peers[*it]->msgbuf.size() > 1)
                            {
                                peers[*it]->msgbuf.back()->set_buffer (write_buf, peers[*it]->get_fd());
                                peers[*it]->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (peers[*it]->get_fd(),
                                                write_buf, peers[*it]->msgbuf.back()) <= 0)
                                {
                                    peers[*it]->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                        
                        it++;
                        
                        for (/* nothing */; it != thecount->peerids.end(); it++)
                        {
                            ss << " ";
                            ss << *it;
                            
                            if (*it != networkidx)
                            {
                                if (peers[*it]->msgbuf.size() > 1)
                                {
                                    peers[*it]->msgbuf.back()->set_buffer (write_buf, peers[*it]->get_fd());
                                    peers[*it]->msgbuf.push_back (new messagebuffer());
                                }
                                else
                                {
                                    if (nbwritebuf (peers[*it]->get_fd(),
                                                    write_buf, peers[*it]->msgbuf.back()) <= 0)
                                    {
                                        peers[*it]->msgbuf.push_back (new messagebuffer());
                                    }
                                }
                            }
                        }
                        
                        message = ss.str();
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        
                        if (clients[i]->msgbuf.size() > 1)
                        {
                            clients[i]->msgbuf.back()->set_buffer (write_buf, clients[i]->get_fd());
                            clients[i]->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (clients[i]->get_fd(), write_buf, clients[i]->msgbuf.back()) <= 0)
                            {
                                clients[i]->msgbuf.push_back (new messagebuffer());
                            }
                        }
                    }
                    else
                    {
                        // send null packet for empty peer list
                        memset (write_buf, 0, BUF_SIZE);
                        
                        if (clients[i]->msgbuf.size() > 1)
                        {
                            clients[i]->msgbuf.back()->set_buffer (write_buf, clients[i]->get_fd());
                            clients[i]->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (clients[i]->get_fd(), write_buf, clients[i]->msgbuf.back()) <= 0)
                            {
                                clients[i]->msgbuf.push_back (new messagebuffer());
                            }
                        }
                    }
                    
                    thecount->clear_peer (networkidx);
                    // send EIcache to the target peer
                    entrywriter* thewriter = clients[i]->get_Icachewriter();
                    filepeer* thepeer = clients[i]->get_Itargetpeer();
                    
                    if (thepeer != NULL)
                    {
                        // send End of Icache message "EIcache"
                        string message = "EIcache ";
                        message.append (clients[i]->get_Icachekey());
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                        }
                        
                        clients[i]->set_Itargetpeer (NULL);
                    }
                    else if (thewriter != NULL)
                    {
                        thewriter->complete();
                        delete thewriter;
                        clients[i]->set_Icachewriter (NULL);
                    }
                    
                    if (thecount->size() == 0)
                    {
                        // clear the count
                        delete thecount;
                        clients[i]->thecount = NULL;
                        // close the client
                        close (clients[i]->get_fd());
                        delete clients[i];
                        clients.erase (clients.begin() + i);
                        i--;
                    }
                }
                else if (strncmp (read_buf, "RWwrite", 7) == 0)
                {
                    // clear the count
                    delete clients[i]->thecount;
                    clients[i]->thecount = NULL;
                    // close the client
                    close (clients[i]->get_fd());
                    delete clients[i];
                    clients.erase (clients.begin() + i);
                    i--;
                }
                else
                {
                    cout << "[fileserver:" << networkidx << "]Debugging: Unknown message";
                }
                
                // enable loop acceleration
                //i--;
                //continue;
            }
        }
        
//gettimeofday(&time_end2, NULL);
//timeslot2 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);
        // receives read/write request or data stream
        for (int i = 0; (unsigned) i < peers.size(); i++)
        {
            // pass and continue when the conneciton to the peer have been closed
            if (peers[i]->get_fd() < 0)
            {
                continue;
            }
            
            int readbytes;
            readbytes = nbread (peers[i]->get_fd(), read_buf);
            
            if (readbytes > 0)
            {
                if (strncmp (read_buf, "Ilook", 5) == 0)       // read request to cache(Icache)
                {
//cout<<"[fileserver:"<<networkidx<<"]Ilook message: "<<read_buf<<endl;
//cout<<"[fileserver:"<<networkidx<<"]network idx: "<<networkidx<<endl;
                    char* token;
                    string cachekey;
                    string appname;
                    string filename;
                    int jobid;
                    string bridgeid;
                    int dstid; // same as bridgeid
                    token = strtok (read_buf, " ");   // <- "Ilook"
                    token = strtok (NULL, " ");   // <- app name
                    appname = token;
                    cachekey = token;
                    cachekey.append ("_");
                    token = strtok (NULL, " ");   // <- file name
                    filename = token;
                    cachekey.append (filename);
                    token = strtok (NULL, " ");   // <- job id
                    jobid = atoi (token);
                    token = strtok (NULL, " ");   // <- bridge id
                    bridgeid = token;
                    dstid = atoi (token);
                    // TODO: restore the remote Icache!!!
                    //dataentry* theentry = thecache->lookup(cachekey);
                    dataentry* theentry = NULL;
                    
                    if (theentry == NULL)     // when the intermediate result is not hit
                    {
                        cout << "\033[0;31mRemote Icache miss\033[0m" << endl;
//cout<<"[fileserver:"<<networkidx<<"]Cache key not hit after receiving Ilook: "<<cachekey<<endl;
                        // send "Imiss"
                        string message = "Imiss ";
                        message.append (bridgeid);
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        
                        if (peers[i]->msgbuf.size() > 1)
                        {
                            peers[i]->msgbuf.back()->set_buffer (write_buf, peers[i]->get_fd());
                            peers[i]->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (peers[i]->get_fd(),
                                            write_buf, peers[i]->msgbuf.back()) <= 0)
                            {
                                peers[i]->msgbuf.push_back (new messagebuffer());
                            }
                        }
                        
                        theentry = thecache->lookup (filename);
                        
                        if (theentry == NULL)     // raw input data is not hit
                        {
                            cout << "\033[0;31mRemote Cache miss\033[0m" << endl;
                            // determine the hash value
                            memset (write_buf, 0, HASHLENGTH);
                            strcpy (write_buf, filename.c_str());
                            uint32_t hashvalue = h (write_buf, HASHLENGTH);
                            // set an entry writer
                            dataentry* newentry = new dataentry (filename, hashvalue);
                            thecache->new_entry (newentry);
                            entrywriter* thewriter = new entrywriter (newentry);
                            // determine the DHT file location
                            int index = hashvalue % nodelist.size();
                            string address = nodelist[index];
                            
                            if (networkidx == index)     // local DHT file
                            {
                                filebridge* thebridge = new filebridge (fbidclock++);
                                bridges.push_back (thebridge);
                                thebridge->set_srctype (DISK);
                                thebridge->set_dsttype (PEER);
                                thebridge->set_entrywriter (thewriter);
                                thebridge->set_dstpeer (peers[i]);
                                thebridge->set_dstid (dstid);
                                thebridge->writebuffer = new msgaggregator (peers[i]->get_fd());
                                bridgeid.append ("\n");
                                thebridge->writebuffer->configure_initial (bridgeid);
                                thebridge->writebuffer->set_msgbuf (&peers[i]->msgbuf);
                                thebridge->writebuffer->set_dwriter (thewriter);
                                // open read file and start sending data to peer
                                thebridge->open_readfile (filename);
                            }
                            else     // remote DHT file
                            {
                                filebridge* thebridge = new filebridge (fbidclock++);
                                bridges.push_back (thebridge);
                                thebridge->set_srctype (PEER);
                                thebridge->set_dsttype (PEER);
                                thebridge->set_entrywriter (thewriter);
                                thebridge->set_dstpeer (peers[i]);
                                thebridge->set_dstid (dstid);
                                // send message to the target peer
                                string message;
                                stringstream ss;
                                ss << "RDread ";
                                ss << filename;
                                ss << " ";
                                ss << thebridge->get_id();
                                message = ss.str();
                                memset (write_buf, 0, BUF_SIZE);
                                strcpy (write_buf, message.c_str());
                                filepeer* thepeer = peers[index];
                                
                                if (thepeer->msgbuf.size() > 1)
                                {
                                    thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                    thepeer->msgbuf.push_back (new messagebuffer());
                                    //continue; // escape the acceleration loop
                                }
                                else
                                {
                                    if (nbwritebuf (thepeer->get_fd(),
                                                    write_buf, thepeer->msgbuf.back()) <= 0)
                                    {
                                        thepeer->msgbuf.push_back (new messagebuffer());
                                        //continue; // escape the acceleration loop
                                    }
                                }
                            }
                        }
                        else     // raw input data is hit
                        {
                            if (theentry->is_being_written())
                            {
                                cout << "\033[0;31mRemote Cache miss\033[0m" << endl;
                                // determine the hash value
                                memset (write_buf, 0, HASHLENGTH);
                                strcpy (write_buf, filename.c_str());
                                uint32_t hashvalue = h (write_buf, HASHLENGTH);
                                // determine the DHT file location
                                int index = hashvalue % nodelist.size();
                                string address = nodelist[index];
                                
                                if (networkidx == index)     // local DHT file
                                {
                                    filebridge* thebridge = new filebridge (fbidclock++);
                                    bridges.push_back (thebridge);
                                    thebridge->set_srctype (DISK);
                                    thebridge->set_dsttype (PEER);
                                    thebridge->set_dstpeer (peers[i]);
                                    thebridge->set_dstid (dstid);
                                    thebridge->writebuffer = new msgaggregator (peers[i]->get_fd());
                                    bridgeid.append ("\n");
                                    thebridge->writebuffer->configure_initial (bridgeid);
                                    thebridge->writebuffer->set_msgbuf (&peers[i]->msgbuf);
                                    // open read file and start sending data to peer
                                    thebridge->open_readfile (filename);
                                }
                                else     // remote DHT file
                                {
                                    filebridge* thebridge = new filebridge (fbidclock++);
                                    bridges.push_back (thebridge);
                                    thebridge->set_srctype (PEER);
                                    thebridge->set_dsttype (PEER);
                                    thebridge->set_dstpeer (peers[i]);
                                    thebridge->set_dstid (dstid);
                                    // send message to the target peer
                                    string message;
                                    stringstream ss;
                                    ss << "RDread ";
                                    ss << filename;
                                    ss << " ";
                                    ss << thebridge->get_id();
                                    message = ss.str();
                                    memset (write_buf, 0, BUF_SIZE);
                                    strcpy (write_buf, message.c_str());
                                    filepeer* thepeer = peers[index];
                                    
                                    if (thepeer->msgbuf.size() > 1)
                                    {
                                        thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                        thepeer->msgbuf.push_back (new messagebuffer());
                                        //continue; // escape the acceleration loop
                                    }
                                    else
                                    {
                                        if (nbwritebuf (thepeer->get_fd(),
                                                        write_buf, thepeer->msgbuf.back()) <= 0)
                                        {
                                            thepeer->msgbuf.push_back (new messagebuffer());
                                            //continue; // escape the acceleration loop
                                        }
                                    }
                                }
                            }
                            else
                            {
                                cout << "\033[0;32m[" << networkidx << "]Remote Cache hit\033[0m" << endl;
                                // prepare the read stream from cache
                                entryreader* thereader = new entryreader (theentry);
                                filebridge* thebridge = new filebridge (fbidclock++);
                                bridges.push_back (thebridge);
                                thebridge->set_srctype (CACHE);
                                thebridge->set_dsttype (PEER);
                                thebridge->set_entryreader (thereader);
                                thebridge->set_dstpeer (peers[i]);
                                thebridge->set_dstid (dstid);
                            }
                        }
                    }
                    else     // when the intermediate result is hit
                    {
//cout<<"[fileserver:"<<networkidx<<"]Cache key hit after receiving Ilook: "<<cachekey<<endl;
                        // send 'Ihit'
                        string message = "Ihit ";
                        message.append (bridgeid);
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        
                        if (peers[i]->msgbuf.size() > 1)
                        {
                            peers[i]->msgbuf.back()->set_buffer (write_buf, peers[i]->get_fd());
                            peers[i]->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (peers[i]->get_fd(),
                                            write_buf, peers[i]->msgbuf.back()) <= 0)
                            {
                                peers[i]->msgbuf.push_back (new messagebuffer());
                            }
                        }
                        
                        // prepare the distribution of intermediate result
                        entryreader* thereader = new entryreader (theentry);
                        filebridge* thebridge = new filebridge (fbidclock++);
                        bridges.push_back (thebridge);
                        thebridge->set_srctype (CACHE);
                        thebridge->set_dsttype (DISTRIBUTE);
                        thebridge->set_entryreader (thereader);
                        thebridge->set_dstpeer (peers[i]);   // to notify the end of distribution
                        thebridge->thecount = new writecount();
                        idistributor* thedistributor = new idistributor ( (&peers), (&iwriters), thebridge->thecount, jobid, networkidx);
                        thebridge->set_distributor (thedistributor);
                    }
                }
                else if (strncmp (read_buf, "RDread", 6) == 0)         // read request to disk
                {
                    string address;
                    string filename;
                    int dstid;
                    string sdstid;
                    char* token;
                    token = strtok (read_buf, " ");   // <- message type
                    token = strtok (NULL, " ");   // <- filename
                    filename = token;
                    token = strtok (NULL, " ");   // <- dstid
                    sdstid = token;
                    dstid = atoi (token);
                    filebridge* thebridge = new filebridge (fbidclock++);
                    bridges.push_back (thebridge);
                    thebridge->set_srctype (DISK);
                    thebridge->set_dsttype (PEER);
                    thebridge->set_dstid (dstid);
                    thebridge->set_dstpeer (peers[i]);
                    thebridge->writebuffer = new msgaggregator (peers[i]->get_fd());
                    sdstid.append ("\n");
                    thebridge->writebuffer->configure_initial (sdstid);
                    thebridge->writebuffer->set_msgbuf (&peers[i]->msgbuf);
                    //thebridge->set_filename(filename);
                    //thebridge->set_dataname(filename);
                    //thebridge->set_dtype(RAW);
                    // open read file and start sending data to client
//cout<<"[fileserver:"<<networkidx<<"]reading file name: "<<filename<<endl;
                    thebridge->open_readfile (filename);
                }
                else if (strncmp (read_buf, "Iread", 5) == 0)
                {
                    char* token;
                    int jobid;
                    int numiblock;
                    int bridgeid;
                    token = strtok (read_buf, " ");   // <- "Iread"
                    token = strtok (NULL, " ");   // <- job id
                    jobid = atoi (token);
                    token = strtok (NULL, " ");   // <- numiblock
                    numiblock = atoi (token);
                    token = strtok (NULL, " ");   // <- bridge id
                    bridgeid = atoi (token);
                    // generate ireader and link to the peer(target: remote peer)
                    ireader* thereader = new ireader (jobid, numiblock, i, bridgeid, PEER);
                    thereader->set_dstpeer (peers[i]);
                    ireaders.push_back (thereader);
                }
                else if (strncmp (read_buf, "Iwrite", 6) == 0)
                {
                    char* token;
                    int jobid;
                    string key;
                    token = strtok (read_buf, " ");   // token <- "Iwrite"
                    token = strtok (NULL, "\n");   // token <- [job id]
                    jobid = atoi (token);
                    iwriter* thewriter = NULL;
                    
                    for (int j = 0; (unsigned) j < iwriters.size(); j++)
                    {
                        if (iwriters[j]->get_jobid() == jobid)
                        {
                            thewriter = iwriters[j];
                            break;
                        }
                    }
                    
                    if (thewriter == NULL)     // no iwriter with the job id
                    {
                        // create new iwriter
                        thewriter = new iwriter (jobid, networkidx);
                        iwriters.push_back (thewriter);
                    }
                    
                    // tokenize first key value pair
                    token = strtok (NULL, " ");
                    
                    while (token != NULL)
                    {
                        key = token;
                        token = strtok (NULL, "\n");   // tokenize value
                        // add the key value pair to the iwriter
                        thewriter->add_keyvalue (key, token);
                        token = strtok (NULL, " ");   // next key
                    }
                }
                else if (strncmp (read_buf, "Owrite", 5) == 0)
                {
                    string record;
                    string address;
                    string filename;
                    char* token;
                    token = strtok (read_buf, " ");   // <- "Owrite"
                    token = strtok (NULL, "\n");   // <- file name
                    filename = token;
                    token += strlen (token) + 1;   // <- token: writing contents
                    record = token;
                    write_file (filename, record);
                }
                else if (strncmp (read_buf, "Eread", 5) == 0)         // end of file read stream notification
                {
                    char* token;
                    int id;
                    int bridgeindex = -1;
                    bridgetype dsttype;
                    token = strtok (read_buf, " ");   // <- Eread
                    token = strtok (NULL, " ");   // <- id of bridge
                    id = atoi (token);
                    
                    for (int j = 0; (unsigned) j < bridges.size(); j++)
                    {
                        if (bridges[j]->get_id() == id)
                        {
                            bridgeindex = j;
                            break;
                        }
                    }
                    
                    if (bridgeindex == -1)
                    {
                        cout << "bridge not found with that index" << endl;
                    }
                    
                    dsttype = bridges[bridgeindex]->get_dsttype();
                    // finish write to cache if writing was ongoing
                    entrywriter* thewriter = bridges[bridgeindex]->get_entrywriter();
                    
                    if (thewriter != NULL)
                    {
                        thewriter->complete();
                        delete thewriter;
                        thewriter = NULL;
                    }
                    
                    if (dsttype == CLIENT)
                    {
//cout<<"\033[0;32mEread received\033[0m"<<endl;
                        file_connclient* theclient = bridges[bridgeindex]->get_dstclient();
                        /*
                                    // clear the client and bridge
                                    for(int j = 0; (unsigned)j < clients.size(); j++)
                                    {
                                      if(clients[j] == theclient)
                                      {
                                        if(theclient->msgbuf.size() > 1)
                                        {
                                          theclient->msgbuf.back()->set_endbuffer(theclient->get_fd());
                                          theclient->msgbuf.push_back(new messagebuffer());
                                        }
                                        else
                                        {
                                          close(clients[j]->get_fd());
                                          delete clients[j];
                                          clients.erase(clients.begin()+j);
                                        }
                                        break;
                                      }
                                    }
                        */
                        // send NULL packet to the client
                        memset (write_buf, 0, BUF_CUT);
                        write_buf[0] = -1;
                        
                        if (theclient->msgbuf.size() > 1)
                        {
//cout<<"\tgoes to buffer"<<endl;
                            theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                            theclient->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
                            {
//cout<<"\tgoes to buffer"<<endl;
                                theclient->msgbuf.push_back (new messagebuffer());
                            }
                            
//else
//cout<<"\tsent directly"<<endl;
                        }
                        
                        // clear the bridge
                        delete bridges[bridgeindex];
                        bridges.erase (bridges.begin() + bridgeindex);
                    }
                    else if (dsttype == PEER)       // stores the data into cache and send data to target node
                    {
                        string message;
                        stringstream ss;
                        ss << "Eread ";
                        ss << bridges[bridgeindex]->get_dstid();
                        message = ss.str();
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;
                        filepeer* thepeer = bridges[bridgeindex]->get_dstpeer();
                        
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                        }
                        
                        // clear the bridge
                        delete bridges[bridgeindex];
                        bridges.erase (bridges.begin() + bridgeindex);
                    }
                    
//cout<<"end of read"<<endl;
                }
                else if (strncmp (read_buf, "Wack", 4) == 0)
                {
//cout<<"Wack"<<endl;
                    string message;
                    char* token;
                    token = strtok (read_buf, " ");   // <- Wack
                    token = strtok (NULL, " ");   // <- write id
                    message = "Wre ";
                    message.append (token);   // append write id
                    // write back immediately
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, message.c_str());
                    
                    if (peers[i]->msgbuf.size() > 1)
                    {
                        peers[i]->msgbuf.back()->set_buffer (write_buf, peers[i]->get_fd());
                        peers[i]->msgbuf.push_back (new messagebuffer());
                        //continue; // escape acceleration loop
                    }
                    else
                    {
                        if (nbwritebuf (peers[i]->get_fd(), write_buf, peers[i]->msgbuf.back()) <= 0)
                        {
                            peers[i]->msgbuf.push_back (new messagebuffer());
                            //continue; // escape acceleration loop
                        }
                    }
                }
                /*
                else if(strncmp(read_buf, "IWack", 5) == 0)
                {
                  string message;
                  char* token;
                
                  token = strtok(read_buf, " "); // <- IWack
                  token = strtok(NULL, " "); // <- write id
                  message = "IWre ";
                  message.append(token); // append write id
                
                  // write back immediately
                  memset(write_buf, 0, BUF_SIZE);
                  strcpy(write_buf, message.c_str());
                
                  if(peers[i]->msgbuf.size() > 1)
                  {
                    peers[i]->msgbuf.back()->set_buffer(write_buf, peers[i]->get_fd());
                    peers[i]->msgbuf.push_back(new messagebuffer());
                    //continue; // escape acceleration loop
                  }
                  else
                  {
                    if(nbwritebuf(peers[i]->get_fd(), write_buf, peers[i]->msgbuf.back()) <= 0)
                    {
                      peers[i]->msgbuf.push_back(new messagebuffer());
                      //continue; // escape acceleration loop
                    }
                  }
                }
                */
                else if (strncmp (read_buf, "Wre", 3) == 0)
                {
                    char* token;
                    int writeid;
                    int clientidx = -1;
                    token = strtok (read_buf, " ");   // <- Wre
                    token = strtok (NULL, " ");   // <- write id
                    writeid = atoi (token);
                    writecount* thecount = NULL;
                    
                    for (int j = 0; (unsigned) j < clients.size(); j++)
                    {
                        if (clients[j]->get_fd() == writeid)
                        {
                            clientidx = j;
                            thecount = clients[j]->thecount;
                        }
                    }
                    
                    if (thecount == NULL)
                    {
                        cout << "[fileserver:" << networkidx << "]Unexpected NULL pointer..." << endl;
                    }
                    
                    thecount->clear_peer (i);
                    
                    if (thecount->peerids.size() == 0)
                    {
                        // send EIcache to the target peer
                        filepeer* thepeer = clients[clientidx]->get_Itargetpeer();
                        
                        if (thepeer != NULL)
                        {
                            // send End of Icache message "EIcache"
                            string message = "EIcache ";
                            message.append (clients[clientidx]->get_Icachekey());
                            memset (write_buf, 0, BUF_SIZE);
                            strcpy (write_buf, message.c_str());
                            
                            if (thepeer->msgbuf.size() > 1)
                            {
                                thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (thepeer->get_fd(),
                                                write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                    thepeer->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                        
                        // close the target client and clear it
                        close (clients[clientidx]->get_fd());
                        delete clients[clientidx];
                        clients.erase (clients.begin() + clientidx);
                    }
                }
                /*
                        else if(strncmp(read_buf, "IWre", 4) == 0)
                        {
                //cout<<"[fileserver:"<<networkidx<<"]Received IWre"<<endl;
                          char* token;
                          int writeid;
                          int bridgeidx = -1;
                
                          token = strtok(read_buf, " "); // <= IWre
                          token = strtok(NULL, " "); // <- write id
                          writeid = atoi(token);
                
                          writecount* thecount = NULL;
                
                          for(int j = 0; (unsigned)j < bridges.size(); j++)
                          {
                            if(bridges[j]->get_id() == writeid)
                            {
                              bridgeidx = j;
                              thecount = bridges[j]->get_dstclient()->thecount;
                              break;
                            }
                          }
                
                          if(thecount == NULL)
                          {
                            cout<<"[fileserver:"<<networkidx<<"]Unexpected NULL pointer..."<<endl;
                          }
                
                          thecount->clear_peer(i);
                
                          if(thecount->peerids.size() == 0)
                          {
                            // notify the end of DISTRIBUTE to peer or client
                            if(bridges[bridgeidx]->get_dstpeer() != NULL)
                            {
                              filepeer* thepeer = bridges[bridgeidx]->get_dstpeer();
                
                              // send Edist message to the dstpeer
                              string message;
                              stringstream ss;
                              ss << "Edist ";
                              ss << bridges[bridgeidx]->get_dstid();
                
                              message = ss.str();
                              memset(write_buf, 0, BUF_SIZE);
                              strcpy(write_buf, message.c_str());
                //cout<<"[fileserver:"<<networkidx<<"]Distribution finished and Edist to be transmitted"<<endl;
                
                              if(thepeer->msgbuf.size() > 1)
                              {
                                thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back(new messagebuffer());
                              }
                              else
                              {
                                if(nbwritebuf(thepeer->get_fd(),
                                      write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                  thepeer->msgbuf.push_back(new messagebuffer());
                                }
                              }
                
                              // clear the entry reader
                              if(bridges[bridgeidx]->get_entryreader() != NULL)
                              {
                                delete bridges[bridgeidx]->get_entryreader();
                                bridges[bridgeidx]->set_entryreader(NULL);
                              }
                            }
                            else if(bridges[bridgeidx]->get_dstclient() != NULL)
                            {
                //cout<<"[fileserver:"<<networkidx<<"]Distribution finished and notification to client to be done"<<endl;
                              // notify the end of DISTRIBUTE to the dstclient
                              file_connclient* theclient = bridges[bridgeidx]->get_dstclient();
                              memset(write_buf, 0, BUF_CUT);
                              write_buf[0] = 1;
                
                              if(theclient->msgbuf.size() > 1)
                              {
                                theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
                                theclient->msgbuf.push_back(new messagebuffer());
                              }
                              else
                              {
                                if(nbwritebuf(theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
                                {
                                  theclient->msgbuf.push_back(new messagebuffer());
                                }
                              }
                            }
                            else
                            {
                              cout<<"Neither dstclient nor dstpeer are set"<<endl;
                            }
                
                            // clear the bridge
                            delete bridges[bridgeidx];
                            bridges.erase(bridges.begin() + bridgeidx);
                          }
                        }
                */
                else if (strncmp (read_buf, "Icache", 6) == 0)
                {
//cout<<"[fileserver:"<<networkidx<<"]Icache message network idx: "<<networkidx<<endl;
                    char* token;
                    int bridgeindex;
                    filebridge* thebridge;
                    string cachekey;
                    token = strtok (read_buf, " ");   // <- "Icache"
                    token = strtok (NULL, " ");   // <- "[app name]"
                    cachekey = token;
                    token = strtok (NULL, "\n");   // <- "[input path]"
                    cachekey.append ("_");
                    cachekey.append (token);
                    thebridge = find_Icachebridge (cachekey, bridgeindex);
                    
                    if (thebridge == NULL)
                    {
                        // determine the hash value
                        uint32_t hashvalue;
                        memset (write_buf, 0, HASHLENGTH);
                        strcpy (write_buf, token);   // token <- file name
                        hashvalue = h (write_buf, HASHLENGTH);
                        thebridge = new filebridge (fbidclock++);
                        bridges.push_back (thebridge);
                        thebridge->set_srctype (PEER);
                        thebridge->set_dsttype (CACHE);
                        thebridge->set_Icachekey (cachekey);
//cout<<"[fileserver:"<<networkidx<<"]Cachekey of new data entry: "<<cachekey<<endl;
                        // set a entry writer
                        dataentry* newentry = new dataentry (cachekey, hashvalue);
                        thecache->new_entry (newentry);
                        entrywriter* thewriter = new entrywriter (newentry);
                        thebridge->set_entrywriter (thewriter);
                        // rest of messages
                        token = strtok (NULL, "");
                        thewriter->write_record (token);
                    }
                    else
                    {
//cout<<"[fileserver:"<<networkidx<<"]Cachekey of written data: "<<cachekey<<endl;
                        // rest of messages
                        token = strtok (NULL, "");
                        thebridge->get_entrywriter()->write_record (token);
                    }
                }
                else if (strncmp (read_buf, "Ihit", 4) == 0)
                {
                    cout << "\033[0;32m[" << networkidx << "]Remote Icache hit\033[0m" << endl;
                    char* token;
                    int dstid;
                    token = strtok (read_buf, " ");   // <- "Ihit"
                    token = strtok (NULL, " ");   // <- bridgeid
                    dstid = atoi (token);
                    filebridge* thebridge = find_bridge (dstid);
                    file_connclient* theclient = thebridge->get_dstclient();
                    memset (write_buf, 0, BUF_CUT);
                    write_buf[0] = 1;
                    
                    if (theclient->msgbuf.size() > 1)
                    {
                        theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                        theclient->msgbuf.push_back (new messagebuffer());
                    }
                    else
                    {
                        if (nbwritebuf (theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
                        {
                            theclient->msgbuf.push_back (new messagebuffer());
                        }
                    }
                }
                else if (strncmp (read_buf, "Imiss", 5) == 0)
                {
                    char* token;
                    filebridge* thebridge;
                    int dstid;
                    token = strtok (read_buf, " ");
                    token = strtok (NULL, " ");
                    dstid = atoi (token);
                    thebridge = find_bridge (dstid);
                    file_connclient* theclient = thebridge->get_dstclient();
                    memset (write_buf, 0, BUF_CUT);
                    write_buf[0] = 0;
                    
                    if (theclient->msgbuf.size() > 1)
                    {
                        theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                        theclient->msgbuf.push_back (new messagebuffer());
                    }
                    else
                    {
                        if (nbwritebuf (theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
                        {
                            theclient->msgbuf.push_back (new messagebuffer());
                        }
                    }
                }
                else if (strncmp (read_buf, "EIcache", 7) == 0)
                {
                    char* token;
                    filebridge* thebridge;
                    int bridgeindex;
                    token = strtok (read_buf, " ");   // <- "EIcache"
                    token = strtok (NULL, "\n");   // <- cachekey
                    thebridge = find_Icachebridge (token, bridgeindex);
                    
                    if (thebridge == NULL)
                    {
                        cout << "[fileserver:" << networkidx << "]Unexpected NULL pointer after receiving \"EIcache\" message" << endl;
                    }
                    else
                    {
//cout<<"[fileserver:"<<networkidx<<"]Completed writing entry with cachekey: "<<token<<endl;
                        thebridge->get_entrywriter()->complete();
                        delete thebridge->get_entrywriter();
                        thebridge->set_entrywriter (NULL);
                        delete thebridge;
                        bridges.erase (bridges.begin() + bridgeindex);
                    }
                }
                else if (strncmp (read_buf, "Edist", 5) == 0)
                {
//cout<<"[fileserver:"<<networkidx<<"]Edist arrived. notifying client end of distribution..."<<endl;
                    char* token;
                    int bridgeid;
                    token = strtok (read_buf, " ");   // <- "Edist"
                    token = strtok (NULL, " ");
                    bridgeid = atoi (token);
                    filebridge* thebridge;
                    int bridgeidx;
                    
                    for (int j = 0; (unsigned) j < bridges.size(); j++)
                    {
                        if (bridges[j]->get_id() == bridgeid)
                        {
                            thebridge = bridges[j];
                            bridgeidx = j;
                        }
                    }
                    
                    if (thebridge == NULL)
                    {
                        cout << "[fileserver:" << networkidx << "]Unexpected filebridge NULL pointer(2)" << endl;
                    }
                    
                    // notify the end of DISTRIBUTE to dstclient
                    file_connclient* theclient = bridges[bridgeidx]->get_dstclient();
                    memset (write_buf, 0, BUF_CUT);
                    write_buf[0] = 1;
                    
                    if (theclient->msgbuf.size() > 1)
                    {
                        theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                        theclient->msgbuf.push_back (new messagebuffer());
                    }
                    else
                    {
                        if (nbwritebuf (theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
                        {
                            theclient->msgbuf.push_back (new messagebuffer());
                        }
                    }
                    
                    // add peers to the write count of the client
                    if (theclient->thecount == NULL)
                    {
                        theclient->thecount = new writecount();
                    }
                    
                    token = strtok (NULL, " ");   // <- first peer ids written
                    
                    while (token != NULL)
                    {
                        theclient->thecount->add_peer (atoi (token));
                        token = strtok (NULL, " ");
                    }
                    
                    // clear the bridge
                    delete bridges[bridgeidx];
                    bridges.erase (bridges.begin() + bridgeidx);
                }
                else     // a filebridge id is passed. this is the case of data read stream
                {
                    filebridge* thebridge;
                    string record;
                    char* token;
                    char* buf;
                    int id;
                    bridgetype dsttype;
                    buf = read_buf;
                    token = strtok (read_buf, "\n");   // <- filebridge id
                    id = atoi (token);
                    buf += strlen (token) + 1;   // <- record
                    record = buf;
                    
                    for (int j = 0; (unsigned) j < bridges.size(); j++)
                    {
                        if (bridges[j]->get_id() == id)
                        {
                            thebridge = bridges[j];
                        }
                    }
                    
                    dsttype = thebridge->get_dsttype();
                    
                    if (dsttype == CLIENT)
                    {
                        // write to cache if writing was ongoing
                        entrywriter* thewriter = thebridge->get_entrywriter();
                        
                        if (thewriter != NULL)
                        {
                            thewriter->write_record (record);
                        }
                        
                        file_connclient* theclient = thebridge->get_dstclient();
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, record.c_str());   // send only the record
                        
//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to a client"<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;
                        if (theclient->msgbuf.size() > 1)
                        {
                            theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                            theclient->msgbuf.push_back (new messagebuffer());
                            //continue; // escape acceleration loop
                        }
                        else
                        {
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: client"<<endl;
                            if (nbwritebuf (theclient->get_fd(),
                                            write_buf, theclient->msgbuf.back()) <= 0)
                            {
                                theclient->msgbuf.push_back (new messagebuffer());
                                //continue; // escape acceleration loop
                            }
                        }
                        
                        //nbwrite(theclient->get_fd(), write_buf);
                    }
                    else if (dsttype == PEER)       // stores the data into cache and send data to target node
                    {
                        // write to cache if writing was ongoing
                        entrywriter* thewriter = thebridge->get_entrywriter();
                        
                        if (thewriter != NULL)
                        {
                            thewriter->write_record (record);
                        }
                        
                        string message;
                        stringstream ss;
                        ss << thebridge->get_dstid();
                        message = ss.str();
                        message.append ("\n");
                        message.append (record);
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
//cout<<endl;
//cout<<"write from: "<<localhostname<<endl;
//cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
//cout<<"message: "<<write_buf<<endl;
//cout<<endl;
                        filepeer* thepeer = thebridge->get_dstpeer();
                        
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                            //continue; // escape acceleration loop
                        }
                        else
                        {
//cout<<endl;
//cout<<"from: "<<localhostname<<endl;
//cout<<"to: "<<thepeer->get_address()<<endl;
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                                //continue; // escape acceleration loop
                            }
                        }
                    }
                }
                
                // enable loop acceleration
                //i--;
                //continue;
            }
            else if (readbytes == 0)
            {
                cout << "[fileserver:" << networkidx << "]Debugging: Connection from a peer disconnected: " << peers[i]->get_address() << endl;
                // clear the peer
                close (peers[i]->get_fd());
                peers[i]->set_fd (-1);
            }
        }
        
//gettimeofday(&time_end2, NULL);
//timeslot3 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);
        // process reading from the disk or cache and send the data to peer or client
        for (int i = 0; (unsigned) i < bridges.size(); i++)
        {
            //for(int accel = 0; accel < 1000; accel++) // accelerate the reading speed by 1000
            //{
            if (bridges[i]->get_srctype() == CACHE)
            {
                if (bridges[i]->get_dsttype() == DISTRIBUTE)
                {
                    bool ret;
                    string record;
                    ret = bridges[i]->get_entryreader()->read_record (record);
                    
                    if (ret)     // successfully read
                    {
                        //string jobdirpath = bridges[i]->get_jobdirpath();
                        //string message;
                        //string value;
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, record.c_str());
                        bridges[i]->get_distributor()->process_message (write_buf, read_buf);
                    }
                    else     // end of the cache contents
                    {
                        // flush and clear the distributor
                        if (bridges[i]->get_distributor() != NULL)
                        {
                            delete bridges[i]->get_distributor();
                            bridges[i]->set_distributor (NULL);
                        }
                        
                        // clear the entryreader
                        delete bridges[i]->get_entryreader();
                        bridges[i]->set_entryreader (NULL);
                        writecount* thecount = bridges[i]->get_dstclient()->thecount;
                        
                        // TODO: if dstpeer exists, thecount should be from bridge itself, not from client
                        // abc
                        if (bridges[i]->get_dstpeer() != NULL)
                        {
                            filepeer* thepeer = bridges[i]->get_dstpeer();
                            // send Edist message to the dstpeer
                            string message;
                            stringstream ss;
                            ss << "Edist ";
                            ss << bridges[i]->get_dstid();
                            
                            // add to the ss the writecount peer information
                            for (set<int>::iterator it = thecount->peerids.begin(); it != thecount->peerids.end(); it++)
                            {
                                ss << " ";
                                ss << *it;
                            }
                            
                            message = ss.str();
                            memset (write_buf, 0, BUF_SIZE);
                            strcpy (write_buf, message.c_str());
                            
                            if (thepeer->msgbuf.size() > 1)
                            {
                                thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (thepeer->get_fd(),
                                                write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                    thepeer->msgbuf.push_back (new messagebuffer());
                                }
                            }
                            
                            // clear the entry reader
                            if (bridges[i]->get_entryreader() != NULL)
                            {
                                delete bridges[i]->get_entryreader();
                                bridges[i]->set_entryreader (NULL);
                            }
                        }
                        else if (bridges[i]->get_dstclient() != NULL)
                        {
                            // notify the end of DISTRIBUTE to the dstclient
                            memset (write_buf, 0, BUF_CUT);
                            write_buf[0] = 1;
                            // determine clientidx
                            int clientidx = -1;
                            
                            for (int j = 0; (unsigned) j < clients.size(); j++)
                            {
                                if (clients[j] == bridges[i]->get_dstclient())
                                {
                                    clientidx = j;
                                    break;
                                }
                            }
                            
                            if (clientidx == -1)
                            {
                                cout << "[fileserver:" << networkidx << "]Cannot find such client" << endl;
                            }
                            
                            if (clients[clientidx]->msgbuf.size() > 1)
                            {
                                clients[clientidx]->msgbuf.back()->set_buffer (write_buf, clients[clientidx]->get_fd());
                                clients[clientidx]->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (clients[clientidx]->get_fd(), write_buf, clients[clientidx]->msgbuf.back()) <= 0)
                                {
                                    clients[clientidx]->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                        else
                        {
                            cout << "Neither dstclient nor dstpeer are set" << endl;
                        }
                        
                        // clear the bridge
                        delete bridges[i];
                        bridges.erase (bridges.begin() + i);
                        i--;
                    }
                }
                else
                {
                    bool ret;
                    string record;
                    memset (write_buf, 0, BUF_SIZE);
                    
                    if (bridges[i]->get_dsttype() == CLIENT)
                    {
                        ret = bridges[i]->get_entryreader()->read_record (record);
                        strcpy (write_buf, record.c_str());
                    }
                    else if (bridges[i]->get_dsttype() == PEER)
                    {
                        stringstream ss;
                        string message;
                        ss << bridges[i]->get_dstid();
                        message = ss.str();
                        message.append ("\n");
                        ret = bridges[i]->get_entryreader()->read_record (record);
                        message.append (record);
                        strcpy (write_buf, message.c_str());
                    }
                    
                    if (ret)     // successfully read
                    {
                        if (bridges[i]->get_dsttype() == CLIENT)
                        {
                            //cout<<endl;
                            //cout<<"write from: "<<localhostname<<endl;
                            //cout<<"write to a client"<<endl;
                            //cout<<"message: "<<write_buf<<endl;
                            //cout<<endl;
                            file_connclient* theclient = bridges[i]->get_dstclient();
                            
                            if (theclient->msgbuf.size() > 1)
                            {
                                theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                                theclient->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                //cout<<endl;
                                //cout<<"from: "<<localhostname<<endl;
                                //cout<<"to: client"<<endl;
                                if (nbwritebuf (theclient->get_fd(),
                                                write_buf, theclient->msgbuf.back()) <= 0)
                                {
                                    theclient->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                        else if (bridges[i]->get_dsttype() == PEER)
                        {
                            //cout<<endl;
                            //cout<<"write from: "<<localhostname<<endl;
                            //cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
                            //cout<<"message: "<<write_buf<<endl;
                            //cout<<endl;
                            filepeer* thepeer = bridges[i]->get_dstpeer();
                            
                            if (thepeer->msgbuf.size() > 1)
                            {
                                thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                //cout<<endl;
                                //cout<<"from: "<<localhostname<<endl;
                                //cout<<"to: "<<thepeer->get_address()<<endl;
                                if (nbwritebuf (thepeer->get_fd(), write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                    thepeer->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                    }
                    else     // no more record
                    {
                        //cout<<"\033[0;33mEread sent!\033[0m"<<endl;
                        delete bridges[i]->get_entryreader();
                        
                        if (bridges[i]->get_dsttype() == CLIENT)
                        {
                            //cout<<"\033[0;32mEread received\033[0m"<<endl;
                            file_connclient* theclient = bridges[i]->get_dstclient();
                            // send NULL packet to the client
                            memset (write_buf, 0, BUF_CUT);
                            write_buf[0] = -1;
                            
                            if (theclient->msgbuf.size() > 1)
                            {
                                //cout<<"\tgoes to buffer"<<endl;
                                theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                                theclient->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
                                {
                                    //cout<<"\tgoes to buffer"<<endl;
                                    theclient->msgbuf.push_back (new messagebuffer());
                                }
                                
                                //else
                                //cout<<"\tsent directly"<<endl;
                            }
                        }
                        else if (bridges[i]->get_dsttype() == PEER)
                        {
                            stringstream ss1;
                            string message1;
                            ss1 << "Eread ";
                            ss1 << bridges[i]->get_dstid();
                            message1 = ss1.str();
                            memset (write_buf, 0, BUF_SIZE);
                            strcpy (write_buf, message1.c_str());
                            //cout<<endl;
                            //cout<<"write from: "<<localhostname<<endl;
                            //cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
                            //cout<<"message: "<<write_buf<<endl;
                            //cout<<endl;
                            filepeer* thepeer = bridges[i]->get_dstpeer();
                            
                            if (thepeer->msgbuf.size() > 1)
                            {
                                thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                //cout<<endl;
                                //cout<<"from: "<<localhostname<<endl;
                                //cout<<"to: "<<thepeer->get_address()<<endl;
                                if (nbwritebuf (thepeer->get_fd(),
                                                write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                    thepeer->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                        
                        delete bridges[i];
                        bridges.erase (bridges.begin() + i);
                        i--;
                        // break the accel loop
                        //break;
                    }
                }
            }
            else if (bridges[i]->get_srctype() == DISK)
            {
                bool is_success;
                string record;
                is_success = bridges[i]->read_record (record);
                
                while (is_success)     // some remaining record
                {
                    if (bridges[i]->writebuffer->add_record (record))
                    {
                        break;
                    }
                    
                    is_success = bridges[i]->read_record (record);
                }
                
                if (!is_success)     // no more record to read
                {
//cout<<"\033[0;33mEread sent!\033[0m"<<endl;
                    // flush the write buffer
                    bridges[i]->writebuffer->flush();
                    // complete writing to cache if writing was ongoing
                    entrywriter* thewriter = bridges[i]->get_entrywriter();
                    
                    if (thewriter != NULL)
                    {
                        thewriter->complete();
                        delete thewriter;
                    }
                    
                    if (bridges[i]->get_dsttype() == CLIENT)
                    {
//cout<<"\033[0;32mEread received\033[0m"<<endl;
                        file_connclient* theclient = bridges[i]->get_dstclient();
                        // send NULL packet to the client
                        memset (write_buf, 0, BUF_CUT);
                        write_buf[0] = -1;
                        
                        if (theclient->msgbuf.size() > 1)
                        {
//cout<<"\tgoes to buffer"<<endl;
                            theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                            theclient->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
                            {
//cout<<"\tgoes to buffer"<<endl;
                                theclient->msgbuf.push_back (new messagebuffer());
                            }
                            
//else
//cout<<"\tsent directly"<<endl;
                        }
                        
                        // clear the bridge
                        delete bridges[i];
                        bridges.erase (bridges.begin() + i);
                    }
                    else if (bridges[i]->get_dsttype() == PEER)
                    {
                        stringstream ss;
                        string message;
                        ss << "Eread ";
                        ss << bridges[i]->get_dstid();
                        message = ss.str();
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        filepeer* thepeer = bridges[i]->get_dstpeer();
                        
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                        }
                        
                        delete bridges[i];
                        bridges.erase (bridges.begin() + i);
                    }
                }
                
                /*
                          if(is_success) // some remaining record
                          {
                            // write to cache if writing was ongoing
                            entrywriter* thewriter = bridges[i]->get_entrywriter();
                            if(thewriter != NULL)
                            {
                              thewriter->write_record(record);
                            }
                
                            if(bridges[i]->get_dsttype() == CLIENT)
                            {
                              memset(write_buf, 0, BUF_SIZE);
                              strcpy(write_buf, record.c_str());
                
                              file_connclient* theclient = bridges[i]->get_dstclient();
                              if(theclient->msgbuf.size() > 1)
                              {
                                theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
                                theclient->msgbuf.push_back(new messagebuffer());
                              }
                              else
                              {
                                if(nbwritebuf(theclient->get_fd(),
                                      write_buf, theclient->msgbuf.back()) <= 0)
                                {
                                  theclient->msgbuf.push_back(new messagebuffer());
                                }
                              }
                
                              //nbwrite(bridges[i]->get_dstclient()->get_fd(), write_buf);
                            }
                            else if(bridges[i]->get_dsttype() == PEER)
                            {
                              stringstream ss;
                              string message;
                              ss << bridges[i]->get_dstid();
                              message = ss.str();
                              message.append(" ");
                              message.append(record);
                
                              memset(write_buf, 0, BUF_SIZE);
                              strcpy(write_buf, message.c_str());
                
                              filepeer* thepeer = bridges[i]->get_dstpeer();
                
                              if(thepeer->msgbuf.size() > 1)
                              {
                                thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back(new messagebuffer());
                              }
                              else
                              {
                                if(nbwritebuf(thepeer->get_fd(),
                                      write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                  thepeer->msgbuf.push_back(new messagebuffer());
                                }
                              }
                
                              //nbwrite(bridges[i]->get_dstpeer()->get_fd(), write_buf);
                            }
                          }
                          else // end of data(file)
                          {
                            // write to cache if writing was ongoing
                            entrywriter* thewriter = bridges[i]->get_entrywriter();
                
                            if(thewriter != NULL)
                            {
                              thewriter->complete();
                              delete thewriter;
                              thewriter = NULL;
                            }
                
                            if(bridges[i]->get_dsttype() == CLIENT)
                            {
                              file_connclient* theclient = bridges[i]->get_dstclient();
                
                              // send NULL packet to the client
                              memset(write_buf, -1, BUF_CUT);
                
                              if(theclient->msgbuf.size() > 1)
                              {
                                theclient->msgbuf.back()->set_buffer(write_buf, theclient->get_fd());
                                theclient->msgbuf.push_back(new messagebuffer());
                              }
                              else
                              {
                                if(nbwritebuf(theclient->get_fd(), write_buf, theclient->msgbuf.back()) <= 0)
                                {
                                  theclient->msgbuf.push_back(new messagebuffer());
                                }
                              }
                
                              // clear the bridge
                              delete bridges[i];
                              bridges.erase(bridges.begin()+i);
                            }
                            else if(bridges[i]->get_dsttype() == PEER)
                            {
                              stringstream ss;
                              string message;
                              ss << "Eread ";
                              ss << bridges[i]->get_dstid();
                              message = ss.str();
                              memset(write_buf, 0, BUF_SIZE);
                              strcpy(write_buf, message.c_str());
                
                //cout<<endl;
                //cout<<"write from: "<<localhostname<<endl;
                //cout<<"write to: "<<bridges[i]->get_dstpeer()->get_address()<<endl;
                //cout<<"message: "<<write_buf<<endl;
                //cout<<endl;
                
                              filepeer* thepeer = bridges[i]->get_dstpeer();
                
                              if(thepeer->msgbuf.size() > 1)
                              {
                                thepeer->msgbuf.back()->set_buffer(write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back(new messagebuffer());
                              }
                              else
                              {
                //cout<<endl;
                //cout<<"from: "<<localhostname<<endl;
                //cout<<"to: "<<thepeer->get_address()<<endl;
                                if(nbwritebuf(thepeer->get_fd(),
                                      write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                  thepeer->msgbuf.push_back(new messagebuffer());
                                }
                              }
                              //nbwrite(bridges[i]->get_dstpeer()->get_fd(), write_buf);
                
                              delete bridges[i];
                              bridges.erase(bridges.begin()+i);
                            }
                
                            // break the accel loop
                            //break;
                          }
                */
            }
            
            //}
        }
        
//gettimeofday(&time_end2, NULL);
//timeslot4 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);
        // process write of iwriters
        for (int i = 0; (unsigned) i < iwriters.size(); i++)
        {
            if (iwriters[i]->is_writing())
            {
                bool ret;
                ret = iwriters[i]->write_to_disk();
                
                if (ret)     // writing finished
                {
                    // send the numblock information to the cache server
                    string message;
                    stringstream ss;
                    ss << "iwritefinish ";
                    ss << iwriters[i]->get_jobid();
                    ss << " ";
                    ss << iwriters[i]->get_numblock();
                    message = ss.str();
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, message.c_str());
                    nbwrite (cacheserverfd, write_buf);
                    // clear the iwriter
                    delete iwriters[i];
                    iwriters.erase (iwriters.begin() + i);
                    i--;
                }
            }
        }
        
        for (int i = 0; (unsigned) i < ireaders.size(); i++)
        {
            bool end = ireaders[i]->read_idata();
            
            if (!end)     // end of reading idata
            {
                delete ireaders[i];
                ireaders.erase (ireaders.begin() + i);
                i--;
            }
        }
        
        // process buffered stream through peers
        for (int i = 0; (unsigned) i < peers.size(); i++)
        {
            while (peers[i]->msgbuf.size() > 1)
            {
                memset (write_buf, 0, BUF_SIZE);
                strcpy (write_buf, peers[i]->msgbuf.front()->get_message().c_str());
                
                if (nbwritebuf (peers[i]->get_fd(), write_buf,
                                peers[i]->msgbuf.front()->get_remain(), peers[i]->msgbuf.front()) > 0)     // successfully transmitted
                {
                    delete peers[i]->msgbuf.front();
                    peers[i]->msgbuf.erase (peers[i]->msgbuf.begin());
                }
                else     // not transmitted completely
                {
                    break;
                }
            }
        }
        
        // process buffered stream through clients
        for (int i = 0; (unsigned) i < clients.size(); i++)
        {
            while (clients[i]->msgbuf.size() > 1)
            {
                if (clients[i]->msgbuf.front()->is_end())
                {
                    //cout<<"\t\t\t\tdon't come here"<<endl;
                    close (clients[i]->get_fd());
                    // send EIcache to the target peer
                    filepeer* thepeer = clients[i]->get_Itargetpeer();
                    
                    if (thepeer != NULL)
                    {
                        // send End of Icache message "EIcache"
                        string message = "EIcache ";
                        message.append (clients[i]->get_Icachekey());
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        
                        if (thepeer->msgbuf.size() > 1)
                        {
                            thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                            thepeer->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (thepeer->get_fd(),
                                            write_buf, thepeer->msgbuf.back()) <= 0)
                            {
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                        }
                    }
                    
                    delete clients[i];
                    clients.erase (clients.begin() + i);
                    i--;
                    break;
                }
                else
                {
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, clients[i]->msgbuf.front()->get_message().c_str());
                    
                    if (nbwritebuf (clients[i]->get_fd(), write_buf,
                                    clients[i]->msgbuf.front()->get_remain(), clients[i]->msgbuf.front()) > 0)     // successfully transmitted
                    {
                        delete clients[i]->msgbuf.front();
                        clients[i]->msgbuf.erase (clients[i]->msgbuf.begin());
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        
//gettimeofday(&time_end2, NULL);
//timeslot6 += time_end2.tv_sec*1000000 + time_end2.tv_usec - time_start2.tv_sec*1000000 - time_start2.tv_usec;
//gettimeofday(&time_start2, NULL);
        /*
            for(int i = 0; (unsigned)i < waitingclients.size(); i++)
            {
              int writeid;
              int countindex = -1;
              writeid = waitingclients[i]->get_writeid();
        
              writecount* thecount = NULL;
        
              for(int j = 0; (unsigned)j < writecounts.size(); j++)
              {
                if(writecounts[j]->get_id() == writeid)
                {
                  thecount = writecounts[j];
                  countindex = j;
                }
              }
        
              if(thecount == NULL)
              {
                // close the client
                close(waitingclients[i]->get_fd());
                delete waitingclients[i];
                waitingclients.erase(waitingclients.begin()+i);
                i--;
              }
              else
              {
                if(thecount->get_count() == 0) // the writing is already cleared
                {
                  // close the client
                  close(waitingclients[i]->get_fd());
                  delete waitingclients[i];
                  waitingclients.erase(waitingclients.begin()+i);
                  i--;
        
                  delete thecount;
                  writecounts.erase(writecounts.begin()+countindex);
                }
                else if(thecount->get_count() > 0)
                {
                  continue;
                }
                else
                {
                  cout<<"[fileserver]Debugging: An abnormal write count."<<endl;
                }
              }
            }
        */
        // listen signal from cache server
        {
            int readbytes;
            readbytes = nbread (cacheserverfd, read_buf);
            
            if (readbytes > 0)
            {
                if (strncmp (read_buf, "boundaries", 10) == 0)
                {
                    string token;
                    double doubletoken;
                    stringstream ss;
                    // raed boundary information and parse it to update the boundary
                    ss << read_buf;
                    ss >> token; // boundaries
                    
                    for (int i = 0; (unsigned) i < nodelist.size(); i++)
                    {
                        ss >> doubletoken;
                        thehistogram->set_boundary (i, doubletoken);
                    }
                }
                else if (strncmp (read_buf, "iwritefinish", 12) == 0)
                {
                    char* token;
                    int jobid;
                    token = strtok (read_buf, " ");   // token <- "iwritefinish"
                    token = strtok (NULL, " ");   // token <- jobid
                    jobid =  atoi (token);
                    
                    for (int i = 0; (unsigned) i < iwriters.size(); i++)
                    {
                        if (iwriters[i]->get_jobid() == jobid)
                        {
                            iwriters[i]->flush();
                            break;
                        }
                    }
                }
                else     // unknown message
                {
                    cout << "[cacheserver]Unknown message from master node" << endl;
                }
            }
            else if (readbytes == 0)
            {
                for (int i = 0; (unsigned) i < clients.size(); i++)
                {
                    cout << "[fileserver:" << networkidx << "]Closing connection to a client..." << endl;
                    close (clients[i]->get_fd());
                }
                
                for (int i = 0; (unsigned) i < peers.size(); i++)
                {
                    close (peers[i]->get_fd());
                }
                
                close (ipcfd);
                close (serverfd);
                close (cacheserverfd);
                cout << "[fileserver:" << networkidx << "]Connection from cache server disconnected. exiting..." << endl;
                return 0;
            }
        }
        thecache->update_size();
    }
    
    return 0;
}

filepeer* fileserver::find_peer (string& address)
{
    for (int i = 0; (unsigned) i < peers.size(); i++)
    {
        if (peers[i]->get_address() == address)
        {
            return peers[i];
        }
    }
    
    cout << "[fileserver:" << networkidx << "]Debugging: No such a peer. in find_peer()" << endl;
    return NULL;
}

filebridge* fileserver::find_bridge (int id)
{
    for (int i = 0; (unsigned) i < bridges.size(); i++)
    {
        if (bridges[i]->get_id() == id)
        {
            return bridges[i];
        }
    }
    
    cout << "[fileserver:" << networkidx << "]Debugging: No such a bridge. in find_bridge(), id: " << id << endl;
    return NULL;
}

filebridge* fileserver::find_Icachebridge (string inputname, int& bridgeindex)
{
    for (int i = 0; (unsigned) i < bridges.size(); i++)
    {
        if (bridges[i]->get_Icachekey() == inputname)
        {
            bridgeindex = i;
            return bridges[i];
        }
    }
    
    bridgeindex = -1;
    return NULL;
}

bool fileserver::write_file (string fname, string& record)
{
    string fpath = dht_path;
    int writefilefd = -1;
    int ret;
    fpath.append (fname);
    writefilefd = open (fpath.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
    
    if (writefilefd < 0)
    {
        cout << "[filebridge]Opening write file failed" << endl;
    }
    
    record.append ("\n");
    // memset(write_buf, 0, BUF_SIZE); <- memset may be not necessary
    strcpy (write_buf, record.c_str());
    ret = write (writefilefd, write_buf, record.length());
    
    if (ret < 0)
    {
        cout << "[fileserver:" << networkidx << "]Writing to write file failed" << endl;
        close (writefilefd);
        return false;
    }
    else
    {
        close (writefilefd);
        return true;
    }
}

//writecount* fileserver::find_writecount(int id, int* idx)
//{
//  for(int i = 0; (unsigned)i < writecounts.size(); i++)
//  {
//    if(writecounts[i]->get_id() == id)
//    {
//      if(idx != NULL)
//        *idx = i;
//      return writecounts[i];
//    }
//  }
//
//  return NULL;
//}

#endif
