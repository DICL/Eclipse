#ifndef __SR_MASTER_HH_
#define __SR_MASTER_HH_

class Master {
 //! Attributes and getters/setters
 protected:
  int port, nslaves, sock;
  Node** backend;

 public:
  Master& set_port (int);
  Master& set_nslaves (int);
  Master& set_port (int);
  Master& set_port (int);
  Master& set_signals ();

 protected:
  bool setup_network ();

  virtual select_slave (uint64_t key); 

 public:
  Master () { } 
  int upload (Order&);
  Order& recv (char* file_name);

};

#endif
