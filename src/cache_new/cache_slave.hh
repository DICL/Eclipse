#pragma once

#include "node.hh"
#include "cache.hh"
#include "iasync_messages.hh"
#include "async_pool.hh"
#include "../common/histogram.hh"
#include "../common/dataentry.hh"
#include "../common/logger.hh"

#include <map>
#include <memory>

class Cache_slave : public Iasync_messages {
  private:
    void async_file_send  (std::string, Node&) override ;
    void async_data_send  (std::string, Node&) override ;
    void async_file_read  (std::string, Node&) override ;
    void async_file_write (std::string, Node&) override ;
                                                         
    void async_request_file_read  (std::string, Node&) override ;
    void async_request_file_write (std::string, Node&) override ;
    void async_request_data_read  (std::string, Node&) override ;
    void async_request_data_write (std::string, Node&) override ;

    int connect_to_master (std::string, int);
    int open_peers_server (int);
    int connect_to_peers (int);
    bool connect_to_ipc (std::string);

    void block_until_event();
    void accept_new_clients();
    void proccess_ipc_requests();
    void proccess_peers_requests();
    void receive_boundaries_from_master();
    void close ();

    int corresponding_node (std::string);

    std::multimap<int, Node*> nodes;
    Logger* log;

    unique_ptr<histogram> thehistogram;
    unique_ptr<Async_pool> pool;
    unique_ptr<Cache<string,string> > cache;

    std::string scratch_path, ip_of_this, master_address, ipc_path;

    int serverfd, masterfd, ipcfd;
    int port;
    int index_of_this;

    const int buffersize = 1 << 23; //! 2^23 = 8MiB
    
  public:
    Cache_slave();
    ~Cache_slave();
    
    bool connect();
    void main_loop();
};
