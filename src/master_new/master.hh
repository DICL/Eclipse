#ifndef __MASTER_HH__
#define __MASTER_HH__

class Master {
  private:
    int port_cache, port_mr, max_job, sock_cache, sock_mr;
    vector<string> nodelist;
    string ipc_path;
    unordered_multiset<Node> nodes;
    unordered_map<int, Job> jobs;

    bool mainloop;
    string next_message();
    int schedule_it();
    void accept_new_connections();

  public:
    Master();
    ~Master() {}

    bool setup_network();
    bool run();
    bool close();
};

#endif
