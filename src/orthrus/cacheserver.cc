#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <mapreduce/definitions.hh>
#include "cacheclient.hh"
#include "iwfrequest.hh"
#include "histogram.hh"
#include <common/settings.hh>

using namespace std;

// this process should be run on master node
// the role of this process is to manage the cache boundaries

int dhtport = -1;

int serverfd = -1;
int ipcfd = -1;
string ipc_path;

int buffersize = 8388608; // 8 MB buffer

vector<string> nodelist;
vector<cacheclient*> clients;
vector<iwfrequest*> iwfrequests;

histogram* thehistogram;

char read_buf[BUF_SIZE]; // read buffer for signal_listener thread
char write_buf[BUF_SIZE]; // write buffer for signal_listener thread

void open_server (int port);

int main (int argc, char** argv)
{
    master_connection themaster; // from <orthrus/cacheclient.hh>
    Settings setted;
    setted.load_settings();
    dhtport = setted.dhtport();
    nodelist = setted.nodelist();
    ipc_path = setted.ipc_path();
    
    if (access (ipc_path.c_str(), F_OK) == 0)
    {
        unlink (ipc_path.c_str());
    }
    
    open_server (dhtport);
    
    if (serverfd < 0)
    {
        cout << "[cacheserver]\033[0;31mOpenning server failed\033[0m" << endl;
        return 1;
    }
    
    struct sockaddr_in connaddr;
    
    int addrlen = sizeof (connaddr);
    
    char* haddrp;
    
    // pre-allocate the clients for order of clients vector
    for (int i = 0; (unsigned) i < nodelist.size(); i++)
    {
        clients.push_back (new cacheclient (nodelist[i]));
    }
    
    int connectioncount = 0;
    
    while ( (unsigned) connectioncount < nodelist.size())
    {
        int fd;
        fd = accept (serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
        
        if (fd < 0)
        {
            cout << "[cacheserver]Accepting failed" << endl;
            // sleep 1 milli second. change this if necessary
            // usleep(1000);
            continue;
        }
        else if (fd == 0)
        {
            cout << "[cacheserver]Accepting failed" << endl;
            exit (1);
        }
        else
        {
            // get ip address of client
            haddrp = inet_ntoa (connaddr.sin_addr);
            
            // find the right index for connected client
            for (int i = 0; clients.size(); i++)
            {
                if (clients[i]->get_address() == haddrp)
                {
                    clients[i]->set_fd (fd);
                    connectioncount++;
                    break;
                }
            }
            
            // set socket to be non-blocking socket to avoid deadlock
            fcntl (fd, F_SETFL, O_NONBLOCK);
        }
    }
    
    // receive master connection
    int tmpfd = accept (ipcfd, NULL, NULL);
    
    if (tmpfd > 0)     // master is connected
    {
        int valid = 1;
        setsockopt (tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
        setsockopt (tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
        setsockopt (tmpfd, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof (valid));
    }
    else
    {
        cout << "[cacheserver]Connection from master is unsuccessful" << endl;
        exit (-1);
    }
    
    // set fd of master
    themaster.set_fd (tmpfd);
    // set the server fd as nonblocking mode
    fcntl (serverfd, F_SETFL, O_NONBLOCK);
    fcntl (tmpfd, F_SETFL, O_NONBLOCK);
    // initialize the EM-KDE histogram
    thehistogram = new histogram (nodelist.size(), NUMBIN);
    // a main iteration loop
    int readbytes = -1;
    int fd;
    struct timeval time_start;
    struct timeval time_end;
    gettimeofday (&time_start, NULL);
    gettimeofday (&time_end, NULL);
    
    while (1)
    {
        fd = accept (serverfd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);
        
        if (fd > 0)     // a client is connected. which will send stop message
        {
            while (readbytes < 0)
            {
                readbytes = nbread (fd, read_buf);
            }
            
            if (readbytes == 0)
            {
                cout << "[cacheserver]Connection abnormally closed from client" << endl;
            }
            else     // a message
            {
                if (strncmp (read_buf, "stop", 4) == 0)
                {
                    for (int i = 0; (unsigned) i < clients.size(); i++)
                    {
                        close (clients[i]->get_fd());
                    }
                    
                    close (serverfd);
                    return 0;
                }
                else     // message other than "stop"
                {
                    cout << "[cacheserver]Unexpected message from client" << read_buf << endl;
                }
            }
        }
        
        // listen to master
        readbytes = nbread (themaster.get_fd(), read_buf);
        
        if (readbytes == 0)
        {
            cout << "[cacheserver]Connection abnormally closed from master(ipc)" << endl;
            usleep (10000);   // 10 msec
        }
        else if (readbytes > 0)       // a message accepted
        {
            if (strncmp (read_buf, "boundaries", 10) == 0)
            {
                // distribute the updated boundaries to each nodes
                for (int i = 0; (unsigned) i < clients.size(); i++)
                {
                    nbwrite (clients[i]->get_fd(), read_buf);
                }
            }
            else if (strncmp (read_buf, "iwritefinish", 12) == 0)
            {
                string message;
                stringstream ss;
                char* token;
                int jobid;
                token = strtok (read_buf, " ");   // token <- "iwritefinish"
                token = strtok (NULL, " ");   // jobid
                jobid = atoi (token);
                // add the request to the vector iwfrequests
                iwfrequest* therequest = new iwfrequest (jobid);
                iwfrequests.push_back (therequest);
                // prepare message for each client
                ss << "iwritefinish ";
                ss << jobid;
                message = ss.str();
                memset (write_buf, 0, BUF_SIZE);
                strcpy (write_buf, message.c_str());
                token = strtok (NULL, " ");
                int peerid;
                
                while (token != NULL)
                {
                    // request to the each peer right after tokenize each peer id
                    peerid = atoi (token);
                    therequest->add_request (peerid);
                    // send message to target client
                    nbwrite (clients[peerid]->get_fd(), write_buf);
                    // tokenize next peer id
                    token = strtok (NULL, " ");
                }
            }
            else     // unknown message
            {
                cout << "[cacheserver]Unknown message from master node";
            }
        }
        
        for (int i = 0; (unsigned) i < clients.size(); i++)
        {
            // do nothing currently
            int readbytes = -1;
            readbytes = nbread (clients[i]->get_fd(), read_buf);
            
            if (readbytes > 0)
            {
                if (strncmp (read_buf, "iwritefinish", 12) == 0)
                {
                    char* token;
                    int jobid;
                    int numblock;
                    token = strtok (read_buf, " ");   // token <- "iwritefinish"
                    token = strtok (NULL, " ");   // token <- jobid
                    jobid = atoi (token);
                    token = strtok (NULL, " ");   // token <- numblock
                    numblock = atoi (token);
                    
                    for (int j = 0; (unsigned) j < iwfrequests.size(); j++)
                    {
                        if (iwfrequests[j]->get_jobid() == jobid)
                        {
                            iwfrequests[j]->add_receive (i, numblock);
                            break;
                        }
                    }
                }
                else
                {
                    cout << "[cacheserver]Abnormal message from clients" << endl;
                }
            }
            else if (readbytes == 0)
            {
                cout << "[cacheserver]Connection to clients abnormally closed" << endl;
                usleep (100000);
            }
        }
        
        for (int i = 0; (unsigned) i < iwfrequests.size(); i++)
        {
            // check whether the request has received all responds
            if (iwfrequests[i]->is_finished())
            {
                // send numblock information in order to the master
                string message;
                stringstream ss;
                ss << "numblocks ";
                ss << iwfrequests[i]->get_jobid();
                
                for (int j = 0; (unsigned) j < iwfrequests[i]->peerids.size(); j++)
                {
                    ss << " ";
                    ss << iwfrequests[i]->numblocks[j];
                }
                
                message = ss.str();
                memset (write_buf, 0, BUF_SIZE);
                strcpy (write_buf, message.c_str());
                nbwrite (themaster.get_fd(), write_buf);
                // clear the iwfrequest
                delete iwfrequests[i];
                iwfrequests.erase (iwfrequests.begin() + i);
                i--;
            }
        }
        
        // sleeps for 1 millisecond
        // usleep(1000);
    }
    
    return 0;
}

void open_server (int port)
{
    struct sockaddr_in serveraddr;
    // socket open
    serverfd = socket (AF_INET, SOCK_STREAM, 0);
    
    if (serverfd < 0)
    {
        cout << "[cacheserver]Socket opening failed" << endl;
    }
    
    int valid = 1;
    setsockopt (serverfd, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof (valid));
    // bind
    memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);
    serveraddr.sin_port = htons ( (unsigned short) port);
    
    if (bind (serverfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0)
    {
        cout << "[cacheserver]\033[0;31mBinding failed\033[0m" << endl;
    }
    
    // listen
    if (listen (serverfd, BACKLOG) < 0)
    {
        cout << "[cacheserver]Listening failed" << endl;
    }
    
    // prepare AF_UNIX socket for the master_connection
    struct sockaddr_un serveraddr2;
    ipcfd = socket (AF_UNIX, SOCK_STREAM, 0);
    
    if (ipcfd < 0)
    {
        cout << "[cacheserver]AF_UNIX socket openning failed" << endl;
        exit (-1);
    }
    
    Settings setted;
    setted.load_settings();

    // bind
    memset ( (void*) &serveraddr2, 0, sizeof (serveraddr2));
    serveraddr2.sun_family = AF_UNIX;
    strcpy (serveraddr2.sun_path, setted.ipc_path().c_str());
    
    if (bind (ipcfd, (struct sockaddr *) &serveraddr2, SUN_LEN (&serveraddr2)) < 0)
    {
        cout << "[cacheserver]IPC Binding fialed" << endl;
        exit (-1);
    }
    
    // listen
    if (listen (ipcfd, BACKLOG) < 0)
    {
        cout << "[cacheserver]Listening failed" << endl;
        exit (-1);
    }
}
