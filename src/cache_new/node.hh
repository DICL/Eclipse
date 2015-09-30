#ifndef __NODE_HH__
#define __NODE_HH__

#include <string>

namespace Node_t {
  enum types {
    PEER = 0,
    APPLICATION = 1,
    MASTER = 2
 };
}; 

class Node {
 protected:
  int fd, index;
  std::string addr;

 public:
  Node ();
  ~Node ();

  Node& set_addr (std::string&);
  Node& set_fd (int);
  Node& set_index (int);

  std::string get_addr ();
  int get_fd ();
  int get_index ();

  bool send (std::string);
  std::string recv ();

  bool operator== (std::string);
};

#endif
