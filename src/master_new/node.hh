#ifndef _NODE_HH_
#define _NODE_HH_

#include "ecfs.hh"
#include <string.h>

enum Node_type {SLAVE, CLIENT};

class Client; //Forward declaration of the other types
class Slave;

class Node {
  private:
    int fd, id; 
    Node_type type;
    string addr;

  public:
    Node() {}
    ~Node() {}

    void operator<< (string in) {
      char write_buf[BUF_SIZE];
      memset (write_buf, 0, BUF_SIZE);
      strcpy (write_buf, in.c_str());
      nbwrite (fd, write_buf);
    }

    bool operator== (Node& that) {
      return this->type == that.type;
    }

    string address
    void close () { ::close(fd); }
};

#endif
