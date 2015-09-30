#include "node.hh"
#include "ecfs.hh"

using namespace std;

// Constructor & destructor {{{
Node::Node(): fd(0), index(-1) { }

Node::~Node() { }
// }}}
// setters & getters {{{
Node& Node::set_addr(std::string& addr) { this->addr = addr; return *this; }
Node& Node::set_fd(int fd)              { this->fd = fd; return *this ; }
Node& Node::set_index(int index)        { this->index = index; return *this; }

std::string Node::get_addr()            { return addr; }
int Node::get_fd()                      { return fd; }
int Node::get_index()                   { return index; }
// }}}
// send  {{{
bool Node::send (std::string input) {
  snbwrite (fd, input);
  return true;
}
// }}}
// recv  {{{
std::string Node::recv () {
  char buffer [BUF_SIZE];
  nbread(fd, buffer); 
  return string(buffer);
}
// }}}
// operator== {{{
bool Node::operator== (std::string s) {
  return s == addr;
}
//}}}
