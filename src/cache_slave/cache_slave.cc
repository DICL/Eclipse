#include "cache_slave.hh"

#include <iostream>
#include <exception>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <iostream>

using std::find;

// Constructor {{{
Cache_slave::Cache_slave()
{
  setted.load();
  networkidx      = -1;
  serverfd        = -1;
  cacheserverfd   = -1;
  ipcfd           = -1;
  fbidclock       = 0; // fb id starts from 0

  scratch_path    = setted.get<string>("path.scratch");
  nodelist        = setted.get<vector<string> > ("network.nodes");
  ipc_path        = setted.get<string> ("path.ipc");
  dhtport         = setted.get<int> ("network.port_cache");
  master_address  = setted.get<string>("network.master");
  localhostname   = setted.getip();
  string logname  = setted.get<string> ("log.name");
  string logtype  = setted.get<string> ("log.type");

  log             = Logger::connect(logname, logtype);

  thehistogram = new histogram (nodelist.size(), NUMBIN);
  thecache = new cache (CACHESIZE);

  networkidx = find (nodelist.begin(), nodelist.end(), localhostname) - nodelist.begin();
}
//}}}
// find_peer {{{
filepeer* Cache_slave::find_peer (string& address) {
    for (auto p : peers) {
      if (p->get_address() == address) return p;
    }

    log->warn ("No such a peer. in find_peer(%s)", address.c_str());
    return NULL;
} //}}}
// find_bridge {{{
filebridge* Cache_slave::find_bridge (int id) {
   for (auto b : bridges) {
     if (b->get_id() == id) return b;
   }

   log->warn ("No such a bridge. in find_peer, id = %d", id);
   return NULL;
} //}}}
// find_Icachebridge {{{
filebridge* Cache_slave::find_Icachebridge (string inputname, int& bridgeindex)
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
} //}}}
// write_file {{{
bool Cache_slave::write_file (string fname, string& record)
{
    string fpath = scratch_path + "/"; // Vicente(solves critical error) 
    int writefilefd = -1;
    int ret;
    fpath.append (fname);
    writefilefd = open (fpath.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
    
    if (writefilefd < 0)
    {
        log->info ("[filebridge]Opening write file failed");
    }
    
    record.append ("\n");
    strcpy (write_buf, record.c_str());
    ret = write (writefilefd, write_buf, record.length());
    
    if (ret < 0)
    {
        log->info ("[Cache_slave:%d]Writing to write file failed", networkidx);
        close (writefilefd);
        return false;
    }
    else
    {
        close (writefilefd);
        return true;
    }
} //}}}
// connect {{{ 
int Cache_slave::connect() {
  if (access (ipc_path.c_str(), F_OK) == EXIT_SUCCESS) unlink (ipc_path.c_str());

  { // connect to cacheserver
    cacheserverfd = -1;
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa; 

    cacheserverfd = socket (AF_INET, SOCK_STREAM, 0); // SOCK_STREAM -> tcp
    if (cacheserverfd < 0) {
      log->info ("Openning socket failed");
      exit (EXIT_FAILURE);
    }

    bzero((void*) &sa, sl);
    sa.sin_family = AF_INET;
    sa.sin_port = htons (dhtport);
    inet_pton (AF_INET, master_address.c_str(), &sa.sin_addr);

    while (EXIT_FAILURE == ::connect (cacheserverfd, reinterpret_cast<sockaddr*> (&sa), sl)) {
      log->info ("Cannot connect to the cache server. Retrying...");
      usleep (100000);
    }

    fcntl (cacheserverfd, F_SETFL, O_NONBLOCK);
    setsockopt (cacheserverfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
    setsockopt (cacheserverfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
  }

  for (int i = 0; i < networkidx; i++) // connect to other peer eclipse nodes
  {
    int clientfd = -1;
    struct sockaddr_in serveraddr;
    struct hostent *hp = NULL;
    
    clientfd = socket (AF_INET, SOCK_STREAM, 0);
    if (clientfd < 0) {
      log->error ("Openning socket failed");
      exit (EXIT_FAILURE);
    }

    hp = gethostbyname (nodelist[i].c_str());
    if (hp == NULL) {
      log->error ("Cannot find host by host name");
      return EXIT_FAILURE;
    }

    memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
    serveraddr.sin_family = AF_INET;
    memcpy (&serveraddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
    serveraddr.sin_port = htons (dhtport);

    while (::connect (clientfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0) {
      log->info ("Cannot connect to: %s", nodelist[i].c_str());
      log->info ("Retrying...");
      usleep (100000);
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

  if (fd < 0) {
    log->error ("Socket opening failed");
    exit (EXIT_FAILURE);
  }

  int valid = 1;
  setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof (valid));

  memset ( (void*) &serveraddr, 0, sizeof (struct sockaddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);
  serveraddr.sin_port = htons ( (unsigned short) dhtport);

  if (bind (fd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0) {
    log->error ("Binding failed");
    exit (EXIT_FAILURE);
  }

  if (listen (fd, BACKLOG) < 0) {
    log->error ("Listening failed");
    exit (EXIT_FAILURE);
  }

  // register the current node itself to the peer list
  peers.push_back (new filepeer (-1, localhostname));

  // register the other peers in order
  for (int i = networkidx + 1; (unsigned) i < nodelist.size(); i++)
    peers.push_back (new filepeer (-1, nodelist[i]));

  // listen connections from peers and complete the eclipse network
  for (int i = networkidx + 1; (unsigned) i < nodelist.size(); i++) {
    struct sockaddr_in connaddr;
    int addrlen = sizeof (connaddr);
    tmpfd = accept (fd, (struct sockaddr *) &connaddr, (socklen_t *) &addrlen);

    if (tmpfd > 0) {
      char* haddrp = inet_ntoa (connaddr.sin_addr);
      string address = haddrp;

      for (int j = networkidx + 1; (unsigned) j < nodelist.size(); j++) {
        if (peers[j]->get_address() == address) {
          peers[j]->set_fd (tmpfd);
        }
      }

      // set the peer fd as nonblocking mode
      fcntl (tmpfd, F_SETFL, O_NONBLOCK);
      setsockopt (tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
      setsockopt (tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));

    } else if (tmpfd < 0) { // retry with same index
      i--;
      continue;

    } else {
      log->info ("connection closed.......................");
    }
  }

  // register the server fd
  serverfd = fd;
  log->info ("Eclipse network successfully established");

  struct sockaddr_un serveraddr2;
  ipcfd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (ipcfd < 0) {
    log->error ("AF_UNIX socket openning failed");
    exit (EXIT_FAILURE);
  }

  memset ( (void*) &serveraddr2, 0, sizeof (serveraddr2));
  serveraddr2.sun_family = AF_UNIX;
  strcpy (serveraddr2.sun_path, ipc_path.c_str());

  if (bind (ipcfd, (struct sockaddr *) &serveraddr2, SUN_LEN (&serveraddr2)) < 0)
  {
    log->error ("IPC Binding failed");
    exit (EXIT_FAILURE);
  }

  if (listen (ipcfd, BACKLOG) < 0)
  {
    log->error ("Listening failed");
    exit (EXIT_FAILURE);
  }

  // set the server fd and ipc fd as nonblocking mode
  fcntl (ipcfd, F_SETFL, O_NONBLOCK);
  fcntl (serverfd, F_SETFL, O_NONBLOCK);
  setsockopt (ipcfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
  setsockopt (ipcfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
  setsockopt (serverfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
  setsockopt (serverfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
  return EXIT_SUCCESS;
}
// }}}
// run_server {{{
int Cache_slave::run_server ()
{
    int tmpfd = -1;
    while (true)
    {
        // Accept local clients {{{
        tmpfd = accept (ipcfd, NULL, NULL);
        
        if (tmpfd > 0)                                       // new file client is connected
        {
            clients.push_back (new file_connclient (tmpfd)); // create new clients
            clients.back()->thecount = new writecount();     // set socket to be non-blocking socket to avoid deadlock

            fcntl (tmpfd, F_SETFL, O_NONBLOCK);
            setsockopt (tmpfd, SOL_SOCKET, SO_SNDBUF, &buffersize, (socklen_t) sizeof (buffersize));
            setsockopt (tmpfd, SOL_SOCKET, SO_RCVBUF, &buffersize, (socklen_t) sizeof (buffersize));
        }
        // }}}
        // For each connected client from IPC socket {{{
        for (int i = 0; (unsigned) i < clients.size(); i++)
        {
            int readbytes = -1;
            readbytes = nbread (clients[i]->get_fd(), read_buf);
            
            if (readbytes > 0)
            {
                char* token;
                string filename;
                
                // The message is either: Rread(raw), Iread(intermediate), Iwrite(intermediate), Owrite(output)
                if (strncmp (read_buf, "Rread", 5) == 0) //{{{
                {
                    string cachekey;
                    string appname;
                    int jobid;
                    token = strtok (read_buf, " ");  // <- "Rread"
                                                     // determine the candidate eclipse node which will have the data
                    string address;
                    token = strtok (NULL, " ");      // <- [app name]
                    appname = token;                 // [app name]
                    cachekey = token;                // [app name]
                    cachekey.append ("_");
                    token = strtok (NULL, " ");      // <- job id
                    jobid = atoi (token);
                    token = strtok (NULL, " ");      // <- file name
                    cachekey.append (token);         // <- cachekey: [app name]_[file name]
                    filename = token;
                                                     // determine the cache location of data
                    memset (read_buf, 0, HASHLENGTH);
                    strcpy (read_buf, filename.c_str());

                    int index;
                    uint32_t hashvalue = h (read_buf, HASHLENGTH);
                    index = thehistogram->get_index (hashvalue);
                    log->debug ("Rread=%s hashvalue=%i index=%i", read_buf, hashvalue, index); 
                    address = nodelist[index];
                    filebridge* thebridge = new filebridge (fbidclock++);
                    bridges.push_back (thebridge);
                    
                    if (index == networkidx)                               // local input data
                    {
                        dataentry* theentry = thecache->lookup (cachekey); // check whether the intermediate result is hit

                        if (theentry == NULL)                              // when the intermediate result is not hit
                        {
                            log->info ("Local Icache miss");
                                                                          
                            memset (write_buf, 0, BUF_CUT);  // send 0 packet to the client to notify that Icache is not hit
                            
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
                                log->info ("Local Cache miss");
                                
                                dataentry* newentry = new dataentry (filename, hashvalue); // set an entry writer
                                thecache->new_entry (newentry);
                                entrywriter* thewriter = new entrywriter (newentry);
                                
                                hashvalue = hashvalue % nodelist.size(); // determine the DHT file location
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
                                    log->info ("Local Cache hit");
                                    thebridge->set_srctype (CACHE);
                                    thebridge->set_dsttype (CLIENT);
                                    thebridge->set_entryreader (thereader);
                                    thebridge->set_dstclient (clients[i]);
                                }
                            }
                        }
                        else     // intermediate result hit
                        {
                            log->info ("Local Icache hit");

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
                    }
                    else     // remote cache peer
                    {
                        // 1. request to the remote cache peer
                        // 2. send it to client
                        thebridge->set_srctype (PEER);
                        thebridge->set_dsttype (CLIENT);
                        thebridge->set_dstclient (clients[i]);

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
                } //}}}
                else if (strncmp (read_buf, "Iread", 5) == 0) //{{{
                {
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
                } //}}}
                else if (strncmp (read_buf, "ICwrite", 6) == 0) // {{{
                {
                    string cachekey;
                    string appname;
                    string inputpath;
                    int jobid;
                    token = strtok (read_buf, " ");                          // <- "ICwrite"
                    token = strtok (NULL, "_");                              // <- ".job"
                    token = strtok (NULL, "_\n");                            // <- job id
                    jobid = atoi (token);
                    token = strtok (NULL, " ");                              // <- [app name]
                    appname = token;                                         // <- [app name]
                    cachekey = token;                                        // <- [app name]
                    cachekey.append ("_");
                    token = strtok (NULL, "\n");                             // <- [input file path]
                    inputpath = token;
                    cachekey.append (inputpath);
                    token = strtok (NULL, "");                               // token -> rest of message

                    entrywriter* thewriter = clients[i]->get_Icachewriter();
                    filepeer* thepeer = clients[i]->get_Itargetpeer();

                    if (thewriter != NULL)                                   // target is local and writing is ongoing
                    {
                        thewriter->write_record (token);
                    }
                    else if (thepeer != NULL)                                // target is remote peer and writing is ongoing
                    {
                        string message = "Icache ";
                        message.append (appname);
                        message.append (" ");
                        message.append (inputpath);
                        message.append ("\n");
                        message.append (token);
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, message.c_str());
                        
                        if (thepeer->msgbuf.size() > 1)                      // send token to the remote cache to store it
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
                    else     // target should be determined, determine the target cache of data
                    {
                        string address;
                        memset (write_buf, 0, HASHLENGTH);
                        strcpy (write_buf, inputpath.c_str());
                        int index;
                        uint32_t hashvalue = h (write_buf, HASHLENGTH);
                        index = thehistogram->get_index (hashvalue);
                        address = nodelist[index];
                        
                        if (index == networkidx)     // local target cache
                        {
                            dataentry* newentry = new dataentry (cachekey, hashvalue); // set a entry writer
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
                            
                            if (thepeer->msgbuf.size() > 1) // send token to the remote cache to store it
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
                    if (clients[i]->thedistributor == NULL) // find or allocate idistributor
                    {
                        clients[i]->thedistributor = new idistributor ( (&peers), (&iwriters), clients[i]->thecount, jobid, networkidx);
                        clients[i]->thedistributor->process_message (token, write_buf);
                    }
                    else         // process the intermediate data
                    {
                        clients[i]->thedistributor->process_message (token, write_buf);
                    }
                } //}}}
                else if (strncmp (read_buf, "Iwrite", 6) == 0) //{{{
                {
                    int jobid;                              // extract job id
                    token = strtok (read_buf, " ");         // <- "Iwrite"
                    token = strtok (NULL, "_");             // <- ".job"
                    token = strtok (NULL, "_\n");           // <- job id
                    jobid = atoi (token);
                    token += strlen (token) + 2;            // jump "_" and " "

                    if (clients[i]->thedistributor == NULL) // find or allocate idistributor
                    {
                        clients[i]->thedistributor = new idistributor ( (&peers), (&iwriters), clients[i]->thecount, jobid, networkidx);
                        clients[i]->thedistributor->process_message (token, write_buf);
                    }
                    else
                    {
                        clients[i]->thedistributor->process_message (token, write_buf); // process the intermediate data
                    }
                } //}}}
                else if (strncmp (read_buf, "Owrite", 6) == 0) //{{{
                {
                    string record;
                    memset (write_buf, 0, BUF_SIZE);
                    strcpy (write_buf, read_buf);
                    token = strtok (read_buf, " ");                // <- "Owrite"
                    token = strtok (NULL, "\n");                   // <- file name
                    filename = token;
                    token += strlen (token) + 1;                   // <- token: writing contents
                    record = token;
                                                                   // determine the target peer node
                    memset (read_buf, 0, HASHLENGTH);
                    strcpy (read_buf, filename.c_str());
                    uint32_t hashvalue = h (read_buf, HASHLENGTH);
                    hashvalue = hashvalue % nodelist.size();

                    if (hashvalue == (unsigned) networkidx)        // write to local disk
                    {
                                                                   // write to the disk
                        write_file (filename, record);
                    }
                    else                                           // remote peer
                    {
                        filepeer* thepeer = peers[hashvalue];      // write to the peer with pre-prepared write_buf
                        
                        if (clients[i]->thecount == NULL)
                        {
                            log->info ("[Cache_slave:%d]The write id is not set before the write request", networkidx);
                        }
                        
                        if (thepeer->msgbuf.size() > 1)            // write to the peer
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
                } //}}}
                else if (strncmp (read_buf, "MWwrite", 7) == 0) //{{{
                {
                    if (clients[i]->thedistributor != NULL) // flush and clear the distributor
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
                        memset (write_buf, 0, BUF_SIZE); // send null packet for empty peer list
                        
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
                    entrywriter* thewriter = clients[i]->get_Icachewriter(); 
                    filepeer* thepeer = clients[i]->get_Itargetpeer();
                    
                    if (thepeer != NULL) // send EIcache to the target peer
                    {
                        string message = "EIcache "; // send End of Icache message "EIcache"
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
                    
                    if (thecount->size() == 0) // clear the count
                    {
                        delete thecount;
                        clients[i]->thecount = NULL;

                        close (clients[i]->get_fd());
                        delete clients[i];
                        clients.erase (clients.begin() + i);
                        i--;
                    }
                } //}}}
                else if (strncmp (read_buf, "RWwrite", 7) == 0) //{{{
                {
                    // clear the count
                    delete clients[i]->thecount;
                    clients[i]->thecount = NULL;
                    // close the client
                    close (clients[i]->get_fd());
                    delete clients[i];
                    clients.erase (clients.begin() + i);
                    i--;
                } //}}}
                else
                {
                    log->error ("Debugging: Unknown message");
                }
            }
        } //}}}
        // Foreach peer, receives read/write request or data stream {{{
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
                        log->info ("Remote Icache miss");
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
                            log->info ("Remote Cache miss");
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
                        else     // raw input data is hit
                        {
                            if (theentry->is_being_written())
                            {
                                log->info ("Remote Cache miss");
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
                                log->info ("Remote Cache hit", networkidx );
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

                    // open read file and start sending data to client
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
                        log->info ("bridge not found with that index");
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
                        file_connclient* theclient = bridges[bridgeindex]->get_dstclient();

                        // send NULL packet to the client
                        memset (write_buf, 0, BUF_CUT);
                        write_buf[0] = -1;
                        
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
                        delete bridges[bridgeindex]; // clear the bridge
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
                        
                        delete bridges[bridgeindex]; // clear the bridge
                        bridges.erase (bridges.begin() + bridgeindex);
                    }
                }
                else if (strncmp (read_buf, "Wack", 4) == 0)
                {
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
                    }
                    else
                    {
                        if (nbwritebuf (peers[i]->get_fd(), write_buf, peers[i]->msgbuf.back()) <= 0)
                        {
                            peers[i]->msgbuf.push_back (new messagebuffer());
                        }
                    }
                }
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
                        log->info ("[Cache_slave:%d]Unexpected NULL pointer...", networkidx );
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
                else if (strncmp (read_buf, "Icache", 6) == 0)
                {
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
                        // rest of messages
                        token = strtok (NULL, "");
                        thebridge->get_entrywriter()->write_record (token);
                    }
                }
                else if (strncmp (read_buf, "Ihit", 4) == 0)
                {
                    log->info ("Remote Icache hit");
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
                        log->info ("[Cache_slave:%d]Unexpected NULL pointer after receiving \"EIcache\" message", networkidx);
                    }
                    else
                    {
                        thebridge->get_entrywriter()->complete();
                        delete thebridge->get_entrywriter();
                        thebridge->set_entrywriter (NULL);
                        delete thebridge;
                        bridges.erase (bridges.begin() + bridgeindex);
                    }
                }
                else if (strncmp (read_buf, "Edist", 5) == 0)
                {
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
                        log->info ("[Cache_slave:%d]Unexpected filebridge NULL pointer(2)", networkidx);
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
                        
                        if (theclient->msgbuf.size() > 1)
                        {
                            theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                            theclient->msgbuf.push_back (new messagebuffer());
                        }
                        else
                        {
                            if (nbwritebuf (theclient->get_fd(),
                                            write_buf, theclient->msgbuf.back()) <= 0)
                            {
                                theclient->msgbuf.push_back (new messagebuffer());
                            }
                        }
                        
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
                        filepeer* thepeer = thebridge->get_dstpeer();
                        
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
            }
            else if (readbytes == 0)
            {
                log->info ("[Cache_slave:%d]Debugging: Connection from a peer disconnected: %s",networkidx, peers[i]->get_address().c_str());
                // clear the peer
                close (peers[i]->get_fd());
                peers[i]->set_fd (-1);
            }
        } //}}}
        // process reading from the disk or cache and send the data to peer or client {{{
        for (int i = 0; (unsigned) i < bridges.size(); i++)
        {
            if (bridges[i]->get_srctype() == CACHE)
            {
                if (bridges[i]->get_dsttype() == DISTRIBUTE)
                {
                    bool ret;
                    string record;
                    ret = bridges[i]->get_entryreader()->read_record (record);
                    
                    if (ret)     // successfully read
                    {
                        memset (write_buf, 0, BUF_SIZE);
                        strcpy (write_buf, record.c_str());
                        bridges[i]->get_distributor()->process_message (write_buf, read_buf);
                    }
                    else     // end of the cache contents
                    {
                        if (bridges[i]->get_distributor() != NULL) // flush and clear the distributor
                        {
                            delete bridges[i]->get_distributor();
                            bridges[i]->set_distributor (NULL);
                        }
                        
                        delete bridges[i]->get_entryreader(); // clear the entryreader
                        bridges[i]->set_entryreader (NULL);
                        writecount* thecount = bridges[i]->get_dstclient()->thecount;
                        
                        // TODO: if dstpeer exists, thecount should be from bridge itself, not from client
                        if (bridges[i]->get_dstpeer() != NULL)
                        {
                            filepeer* thepeer = bridges[i]->get_dstpeer();
                            string message; // send Edist message to the dstpeer
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
                            
                            if (bridges[i]->get_entryreader() != NULL) // clear the entry reader
                            {
                                delete bridges[i]->get_entryreader();
                                bridges[i]->set_entryreader (NULL);
                            }
                        }
                        else if (bridges[i]->get_dstclient() != NULL) // notify the end of DISTRIBUTE to the dstclient
                        {
                            memset (write_buf, 0, BUF_CUT);
                            write_buf[0] = 1;
                            int clientidx = -1;
                            
                            for (int j = 0; (unsigned) j < clients.size(); j++) // determine clientidx
                            {
                                if (clients[j] == bridges[i]->get_dstclient())
                                {
                                    clientidx = j;
                                    break;
                                }
                            }
                            if (clientidx == -1)
                            {
                                log->info ("Cannot find such client");
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
                            log->info ("Neither dstclient nor dstpeer are set");
                        }
                        
                        delete bridges[i]; // clear the bridge
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
                            file_connclient* theclient = bridges[i]->get_dstclient();
                            
                            if (theclient->msgbuf.size() > 1)
                            {
                                theclient->msgbuf.back()->set_buffer (write_buf, theclient->get_fd());
                                theclient->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (theclient->get_fd(),
                                                write_buf, theclient->msgbuf.back()) <= 0)
                                {
                                    theclient->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                        else if (bridges[i]->get_dsttype() == PEER)
                        {
                            filepeer* thepeer = bridges[i]->get_dstpeer();
                            
                            if (thepeer->msgbuf.size() > 1)
                            {
                                thepeer->msgbuf.back()->set_buffer (write_buf, thepeer->get_fd());
                                thepeer->msgbuf.push_back (new messagebuffer());
                            }
                            else
                            {
                                if (nbwritebuf (thepeer->get_fd(), write_buf, thepeer->msgbuf.back()) <= 0)
                                {
                                    thepeer->msgbuf.push_back (new messagebuffer());
                                }
                            }
                        }
                    }
                    else     // no more record
                    {
                        delete bridges[i]->get_entryreader();
                        
                        if (bridges[i]->get_dsttype() == CLIENT)
                        {
                            file_connclient* theclient = bridges[i]->get_dstclient();
                            memset (write_buf, 0, BUF_CUT); // send NULL packet to the client
                            write_buf[0] = -1;
                            
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
                        else if (bridges[i]->get_dsttype() == PEER)
                        {
                            stringstream ss1;
                            string message1;
                            ss1 << "Eread ";
                            ss1 << bridges[i]->get_dstid();
                            message1 = ss1.str();
                            memset (write_buf, 0, BUF_SIZE);
                            strcpy (write_buf, message1.c_str());
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
                        }
                        
                        delete bridges[i];
                        bridges.erase (bridges.begin() + i);
                        i--;
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
                        file_connclient* theclient = bridges[i]->get_dstclient();
                        // send NULL packet to the client
                        memset (write_buf, 0, BUF_CUT);
                        write_buf[0] = -1;
                        
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
            }
        } //}}}
        // process write of iwriters {{{
        for (int i = 0; (unsigned) i < iwriters.size(); i++)
        {
            if (iwriters[i]->is_writing() && iwriters[i]->write_to_disk())
            {
                //bool ret = iwriters[i]->write_to_disk();
                
                //if (ret)                              // writing finished
                //{                                     // send the numblock information to the cache server
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
                    delete iwriters[i];               // clear the iwriter
                    iwriters.erase (iwriters.begin() + i);
                    i--;
                //}
            }
        } // }}}
        // Read data from ireaders {{{
        for (int i = 0; (unsigned) i < ireaders.size(); i++)
        {
            bool end = ireaders[i]->read_idata();
            
            if (!end)     // end of reading idata
            {
                delete ireaders[i];
                ireaders.erase (ireaders.begin() + i);
                i--;
            }
        } //}}}
        // process buffered stream through peers {{{
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
        } // }}}
        // process buffered stream through clients {{{
        for (int i = 0; (unsigned) i < clients.size(); i++)
        {
            while (clients[i]->msgbuf.size() > 1)
            {
                if (clients[i]->msgbuf.front()->is_end())
                {
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
        } // }}}
        // listen signal from cache server {{{
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
                    ss << read_buf; // read boundary information and parse it to update the boundary
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
                    log->info ("Unknown message from master node");
                }
            }
            else if (readbytes == 0)
            {
                for (int i = 0; (unsigned) i < clients.size(); i++)
                {
                    log->info ("Closing connection to a client...");
                    close (clients[i]->get_fd());
                }
                
                for (int i = 0; (unsigned) i < peers.size(); i++)
                {
                    close (peers[i]->get_fd());
                }
                
                close (ipcfd);
                close (serverfd);
                close (cacheserverfd);
                log->info ("Connection from cache server disconnected. exiting...");
                return 0;
            }
        } //}}}
        thecache->update_size();
    } 
    return EXIT_SUCCESS;
} // }}}
