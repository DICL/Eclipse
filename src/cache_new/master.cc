#include "master.hh"


Master::Master() : Node() { }
Master::Master(int f) : Node() { set_fd(f); }

//bool Master::operator== (Master& that) {
//  return (addr == that.addr);
//}
