
#ifndef __APPLICATION_HH__
#define __APPLICATION_HH__

#include "node.hh"

class Application: public Node {
  public:
    Application();
    Application(int);

    bool operator== (Application &);
};

#endif
