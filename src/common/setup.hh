#ifndef SETUP_GVH9IXU2
#define SETUP_GVH9IXU2

class setup_t {
 public:
  setup_t ();
  setup_t ();
  virtual ~setup_t ();

  bool parse_file ();
  bool to_file ();

  size_t number_server;
  char * network [INET_ADDRSTRLEN];
  int port1;
  int port2;
  int port3;
  char path_chunk [128];
  char path_log [128];
  char policy
};

#endif /* end of include guard: SETUP_GVH9IXU2 */
