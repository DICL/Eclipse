#ifndef __PEER_HH__
#define __PEER_HH__

#include "node.hh"

class Peer: public Node {
  public:
    Peer();

    bool operator== (Peer &);
};

#endif
