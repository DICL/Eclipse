#include "cache_slave.hh"

#include <vector>
#include <thread>
#include <future>
#include <iostream>
#include <string.h>
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
#include <linux/limits.h>
#include <utility>      // std::pair, std::get

#include "master.hh"
#include "peer.hh"
#include "application.hh"
#include "cache_vector.hh"

using namespace Node_t;
using namespace std;
using namespace placeholders;

using vec_str    = std::vector<std::string>;
using vec_node   = std::vector<Node*>;
using PAIR       = std::pair<int, Node*>;
auto socketopt   = std::bind (&setsockopt, _1, SOL_SOCKET, _2, &_3, sizeof _3);

// Auxiliar functions {{{
auto get_range_of = [] (const multimap<int, Node*>& m, int type) -> vec_node {
  vec_node vec;
  auto it = m.equal_range (type);
  transform (it.first, it.second, back_inserter(vec), [] (PAIR p) {
              return (p.second);
            });
  return vec;
};

int Cache_slave::corresponding_node (string s) {
  uint32_t hashvalue = h (s.c_str(), HASHLENGTH);
  return thehistogram->get_index (hashvalue);
}
// }}}
// Cache_slave {{{
Cache_slave::Cache_slave() {
  Settings setted = Settings().load();

  scratch_path     = setted.get<string> ("path.scratch");
  vec_str nodelist = setted.get<vec_str > ("network.nodes");
  ip_of_this       = setted.getip();
  ipc_path         = setted.get<string> ("path.ipc");
  port             = setted.get<int> ("network.port_cache");
  master_address   = setted.get<string> ("network.master");
  string logname   = setted.get<string> ("log.name");
  string logtype   = setted.get<string> ("log.type");

  log              = Logger::connect (logname, logtype);

  index_of_this = find (nodelist.begin(), nodelist.end(), ip_of_this) - nodelist.begin();

  for (string node : nodelist) {
    Peer* peer = new Peer();
    peer->set_addr(node);

    nodes.insert (make_pair(Node_t::PEER, peer));
  }
  thehistogram.reset (new histogram (nodelist.size(), NUMBIN));
  pool.reset (new Async_pool (4));
  cache.reset (new Cache_vector (CACHESIZE));
}
// }}}
// ~Cache_slave {{{
Cache_slave::~Cache_slave() {
  Logger::disconnect(log);
}
// }}}
// connect {{{
bool Cache_slave::connect() {
  auto fut = std::async (launch::async, &Cache_slave::open_peers_server, this, port);
  sleep(5);

  connect_to_peers (port);
  fut.wait();

  log->info ("Eclipse Cache completed. (Node=%s, Master=%s, buffersize=%ld, port=%d)",
             ip_of_this.c_str(), master_address.c_str(), buffersize, port);
  //connect_to_master (master_address, port);
  connect_to_ipc (ipc_path);

  return true;
}
// }}}
// main_loop {{{
void Cache_slave::main_loop() {
  while (true) {
    accept_new_clients();
    block_until_event();
    proccess_ipc_requests();
    proccess_peers_requests();
    sleep(1);
//    receive_boundaries_from_master();
  }
}
//}}}

// ------------------ PRIVATE METHODS ----------------------

// connect_to_master {{{
int Cache_slave::connect_to_master(std::string addr, int port) {
    struct sockaddr_in sa {0};
    socklen_t sl = sizeof sa;

    int masterfd = socket (AF_INET, SOCK_STREAM, 0);
    log->panic_if (masterfd == EXIT_FAILURE, "Openning socket failed");

    sa.sin_family = AF_INET;
    sa.sin_port = htons (port);
    inet_pton (AF_INET, addr.c_str(), &sa.sin_addr);

    while (0 > ::connect (masterfd, reinterpret_cast<sockaddr*> (&sa), sl)) {
      log->info ("Cannot connect to the cache server. Retrying...");
      usleep (100000);
    }

    fcntl (masterfd, F_SETFL, O_NONBLOCK);
    socketopt (masterfd, SO_SNDBUF, buffersize);
    socketopt (masterfd, SO_RCVBUF, buffersize);
    nodes.insert (std::make_pair (MASTER, new Master (masterfd)));

    return masterfd;
}
// }}}
// connect_to_ipc {{{
bool Cache_slave::connect_to_ipc(std::string ipc_path) {
  if (access (ipc_path.c_str(), F_OK) == EXIT_SUCCESS) unlink (ipc_path.c_str());

  struct sockaddr_un sa {0};
  ipcfd = socket (AF_UNIX, SOCK_STREAM, 0);
  log->panic_if (ipcfd == EXIT_FAILURE, "AF_UNIX socket openning failed");

  sa.sun_family = AF_UNIX;
  ipc_path.copy (sa.sun_path, PATH_MAX);

  int ret = bind (ipcfd, (sockaddr*) &sa, SUN_LEN (&sa));
  log->panic_if (ret == EXIT_FAILURE, "IPC Binding failed");

  ret = listen (ipcfd, BACKLOG);
  log->panic_if (ret == EXIT_FAILURE, "Listening failed");

  // set the server fd and ipc fd as nonblocking mode
  fcntl (ipcfd, F_SETFL, O_NONBLOCK);
  socketopt (ipcfd, SO_SNDBUF, buffersize);
  socketopt (ipcfd, SO_RCVBUF, buffersize);

  return true;
}
// }}}
// open_peers_server {{{
int Cache_slave::open_peers_server (int port) {
  struct sockaddr_in sa {0};

  int fd = socket (AF_INET, SOCK_STREAM, 0);
  log->panic_if (fd == EXIT_FAILURE, "Socket opening failed");

  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl (INADDR_ANY);
  sa.sin_port = htons (port);

  int valid = 1;
  socketopt (fd, SO_REUSEADDR, valid);

  if (bind (fd, reinterpret_cast<sockaddr*>(&sa), sizeof sa) == EXIT_FAILURE)
    log->panic ("Binding failed");

  if (listen (fd, BACKLOG) == EXIT_FAILURE)
    log->panic ("Listening failed");

  // listen connections from peers and complete the eclipse network
  unsigned i = 1;
  while (i < get_range_of(nodes, Node_t::PEER).size()) {
    sockaddr_in sa;
    socklen_t sl = sizeof sa;

    int tmpfd = accept (fd, (sockaddr*) &sa, &sl);
    if (tmpfd > 0) {

      std::string address = inet_ntoa (sa.sin_addr);

      auto peers = get_range_of (nodes, Node_t::PEER);
      auto it = find_if (peers.begin(), peers.end(), [&address] (Node* n) {
                  return *n == address;
                });

      if (it != peers.end()) {
        fcntl (tmpfd, F_SETFL, O_NONBLOCK); // set the peer fd as nonblocking mode
        socketopt (tmpfd, SO_SNDBUF, buffersize);
        socketopt (tmpfd, SO_RCVBUF, buffersize);

        (*it)->set_fd (tmpfd);
        i++;
        log->debug ("Got one peer=%s", address.c_str());
      }
    }
  }

  fcntl (fd, F_SETFL, O_NONBLOCK);
  return  fd;
}
// }}}
// connect_to_peers {{{
int Cache_slave::connect_to_peers(int port) {
  for (auto peer : get_range_of (nodes, Node_t::PEER)) {
    if (peer->get_addr() == ip_of_this)
      continue;

    char addr_name [NI_MAXHOST] {0}, service [NI_MAXSERV] {0};

    struct addrinfo hints = {0}, *result;

    peer->get_addr().copy(addr_name, NI_MAXHOST);
    snprintf (service, NI_MAXSERV, "%d", port);

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_NUMERICSERV;

    int ret = getaddrinfo (addr_name, service, &hints, &result); //! Lookup the dns
    log->panic_if (ret != 0, "Address[%s] not found[err=%s]",
                   addr_name, gai_strerror (ret));

    int clientfd = socket (AF_INET, SOCK_STREAM, 0);
    log->panic_if (clientfd == EXIT_FAILURE, "Openning socket failed");

    while (::connect (clientfd, result->ai_addr, result->ai_addrlen) < 0) {
      log->info ("Cannot connect to: %s, Retrying...", addr_name);
      usleep (100000);
    }

    freeaddrinfo(result);

    fcntl (clientfd, F_SETFL, O_NONBLOCK); // set the peer fd as nonblocking mode
    socketopt (clientfd, SO_SNDBUF, buffersize);
    socketopt (clientfd, SO_RCVBUF, buffersize);

    peer->set_fd (clientfd);
  }
  return true;
}
// }}}
// block_until_event {{{
void Cache_slave::block_until_event() {

}
// }}}
// accept_new_clients {{{
void Cache_slave::accept_new_clients() {
  int tmpfd = accept (ipcfd, NULL, NULL);

  if (tmpfd > 0) {
    nodes.insert (make_pair (Node_t::APPLICATION, new Application(tmpfd)));
    fcntl (tmpfd, F_SETFL, O_NONBLOCK);
    socketopt (tmpfd, SO_SNDBUF, buffersize);
    socketopt (tmpfd, SO_RCVBUF, buffersize);
  }
}
// }}}
// proccess_ipc_requests {{{
void Cache_slave::proccess_ipc_requests() {
  map<string, function<void(string)> > ipc_functions { //{{{
    {
      "Rinput", [this](string s) { 
        string app, filename;
        int jobid;
        stringstream ss (s);
  
        ss >> app >> jobid >> filename;  

        Node* node = get_range_of (nodes, Node_t::PEER)[corresponding_node (filename)];

        if (node == ip_of_this) {
          if (cache.exits (filename)) {
            pool->add_task (async_send_file, this, filename, application);

          } else {
            pool->add_task (async_load_file, this, filename, application);
          }

        } else {
          async_request_file_read (filename, node);
        }
      }
    },{
      "Iread", [this](string s) {
         int jobid, peerid, numiblock;
         stringstream ss (s);

         ss >> jobid >> peerid >> numiblock;
         
         if (peerid == id_of_this) {
            async_send_data (numiblock, application);

         } else {
            async_request_data_read (filename, application);
         }
       }
    },{
      "Iwrite", [this](string s) {
        string jobid, keyvalue, key, value;
        stringstream ss (s);

        getline (ss, jobid, '\n');
        getline (ss, keyvalue, '\n'); 

        ss.str (keyvalue);
        ss >> key >> value;
        
        if (corresponding_node(key) == id_of_this) {
          thecache[key] = value;                        // :TODO:
  
        } else {
          async_request_data_write (key, value);
        }
      }
    },{
      "Owrite", [this](string s) {
        string filename, data;
        stringstream ss (s);
        
        ss >> filename >> data;
      
        if (corresponding_node(filename) == id_of_this) {
          async_file_write (filename, data);

        } else {
          async_request_file_write (filename, data);
        }
      }
    }
  }; //}}}

  for (auto application : get_range_of (nodes, Node_t::APPLICATION)) {
    string input = application->recv();
    string header = input.substr (0, input.find_first_of(' '));
    string body   = input.substr (input.find_first_of(' '));
    
    ipc_functions[header](body);
  }
}
// }}}
// proccess_peers_requests {{{
void Cache_slave::proccess_peers_requests() {

}
// }}}
// receive_boundaries_from_master {{{
void Cache_slave::receive_boundaries_from_master() {
  auto Master = get_range_of (nodes, Node_t::MASTER).back(); 
  string boundaries = Master->recv();

  if (not boundaries.empty()) {
      stringstream ss (boundaries);
      string header;
      ss >> header ; // boundaries/iwritefinish token

      if (header == "boundaries") {
        double doubletoken;

        auto vec = get_range_of (nodes, Node_t::PEER);

        for (decltype(vec.size()) i = 0; i < vec.size(); i++) {
          ss >> doubletoken;
          thehistogram->set_boundary (i, doubletoken);
        }

      } else if (header == "iwritefinish") {
                //Do nothing now

      } else { // unknown message {
        log->info ("Unknown message from master node");
      }

  } else {
    this->close();
  }
  //thecache->update_size();
}
// }}}
// close {{{
void Cache_slave::close() {
  for (auto it : nodes) {
    log->info ("Closing connection to a client...");
    ::close (it.second->get_fd());
  }
  log->info ("Connection from cache server disconnected. exiting...");
}
