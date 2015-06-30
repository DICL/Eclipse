#include "client.hh"
#include "../common/ecfs.hh"
#include "../common/settings.hh"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>

using namespace std;

char read_buf[BUF_SIZE];
char write_buf[BUF_SIZE];
char master_address[BUF_SIZE];
int port = -1;
int dhtport = -1;
int masterfd = -1;
bool master_is_set = false;
vector<string> nodelist;


int main (int argc, char** argv)
{
    // usage
    if (argc < 2)
    {
        cout << "Insufficient arguments: at least 1 argument needed" << endl;
        cout << "usage: client [request]" << endl;
        cout << "Exiting..." << endl;
        return 1;
    }
    
    string token;
    Settings setted;
    setted.load();
    port = setted.get<int>("network.port_mapreduce");
    dhtport = setted.get<int> ("network.port_cache");
    strcpy (master_address, setted.get<string>("network.master").c_str());
    master_is_set = true;

    // verify initialization
    if (port == -1)
    {
        cout << "[client]port should be specified in the setup.conf" << endl;
        exit (1);
    }
    
    if (master_is_set == false)
    {
        cout << "[client]master_address should be specified in the setup.conf" << endl;
        exit (1);
    }
    
    nodelist = setted.get<vector<string> > ("network.nodes");
    
    // copy request command to write buffer
    if (strncmp (argv[1], "stop", 4) == 0)
    {
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, "stop");
    }
    else if (strncmp (argv[1], "numslave", 8) == 0)
    {
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, "numslave");
    }
    else if (strncmp (argv[1], "numclient", 9) == 0)
    {
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, "numclient");
    }
    else if (strncmp (argv[1], "numjob", 6) == 0)
    {
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, "numjob");
    }
    else if (strncmp (argv[1], "help", 4) == 0)
    {
        // TODO: lists request and their usage
        exit (0);
    }
    else
    {
        memset (write_buf, 0, BUF_SIZE);
        strcpy (write_buf, argv[1]);
    }
    
    masterfd = connect_to_server (master_address, port);
    
    if (masterfd < 0)
    {
        cout << "Connecting to master failed" << endl;
        exit (1);
    }
    
    // set sockets to be non-blocking socket to avoid deadlock
    fcntl (masterfd, F_SETFL, O_NONBLOCK);
    // start listener thread
    pthread_t listener_thread;
    pthread_create (&listener_thread, NULL, signal_listener, (void*) &masterfd);
    
    // sleeping loop which prevents process termination
    while (1)
    {
        sleep (1);
    }
    
    return 0;
}

int connect_to_server (char *host, unsigned short port)
{
    int clientfd;
    struct sockaddr_in serveraddr;
    struct hostent *hp;
    // SOCK_STREAM -> tcp
    clientfd = socket (AF_INET, SOCK_STREAM, 0);
    
    if (clientfd < 0)
    {
        cout << "Openning socket failed" << endl;
        exit (1);
    }
    
    hp = gethostbyname (host);
    
    if (hp == NULL)
    {
        cout << "Cannot find host by host name" << endl;
        return -1;
    }
    
    memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
    serveraddr.sin_family = AF_INET;
    memcpy (&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
    serveraddr.sin_port = htons (port);
    connect (clientfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr));
    return clientfd;
}

void* signal_listener (void* args)
{
    int serverfd = * ( (int*) args);
    int readbytes = 0;
    char tmp_buf[BUF_SIZE];
    
    while (1)
    {
        readbytes = nbread (serverfd, read_buf);
        
        if (readbytes == 0)     // connection closed from master
        {
            cout << "Connection from master is abnormally closed" << endl;
            close (serverfd);
            exit (0);
        }
        else if (readbytes < 0)       // no signal arrived
        {
            continue;
        }
        else     // a signal arrived from master
        {
            if (strncmp (read_buf, "whoareyou", 9) == 0)
            {
                // respond to "whoareyou"
                memset (tmp_buf, 0, strlen ("client") + 1);
                strcpy (tmp_buf, "client");
                nbwrite (serverfd, tmp_buf);
                // request to master
                nbwrite (serverfd, write_buf);
                
                if (strncmp (write_buf, "stop", 4) == 0)       // if argument is "stop"
                {
                    // send close message to cacheserver
                    int fd;
                    fd = connect_to_server (master_address, dhtport);
                    
                    if (fd <= 0)
                    {
                        cout << "[client]Error occured during the connection to the cacheserver" << endl;
                    }
                    
                    nbwrite (fd, write_buf);
                    close (fd);
                    /*
                    for(int i = 0; (unsigned)i < nodelist.size(); i++)
                    {
                      memset(tmp_buf, 0, nodelist[i].length() + 1);
                      strcpy(tmp_buf, nodelist[i].c_str());
                      fd = connect_to_server(tmp_buf, dhtport);
                      nbwrite(fd, write_buf);
                      close(fd);
                    }
                    */
                }
            }
            else if (strncmp (read_buf, "close", 5) == 0)
            {
                cout << "Close request from master" << endl;
                close (serverfd);
                cout << "Exiting client..." << endl;
                exit (0);
            }
            else if (strncmp (read_buf, "result", 6) == 0)
            {
                cout << read_buf << endl;
                close (serverfd);
                exit (0);
            }
            else
            {
                cout << "Signal from master: " << read_buf << endl;
            }
        }
        
        // sleeps for 0.0001 seconds. change this if necessary
        // usleep(100);
    }
    
    close (serverfd);
    cout << "Exiting client..." << endl;
    exit (0);
}
