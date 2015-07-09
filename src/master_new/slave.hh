
class Slave: public Node {
  private:
    int maxmaptasks, maxreducetask;

  public:
    Slave() : Node(SLAVE) {}

};

