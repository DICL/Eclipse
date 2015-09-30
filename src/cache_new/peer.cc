#include "peer.hh"


Peer::Peer() : Node() { }

bool Peer::operator== (Peer& that) {
  return (addr == that.addr);
}
