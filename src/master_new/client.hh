#include "node.hh"

class Client: public Node {
  private:
    Job* job;

  public:
    Client () : Node(CLIENT) {}

    bool recieve () {
      char write_buf[BUF_SIZE];
      memset (write_buf, 0, BUF_SIZE);
      strcpy (write_buf, in.c_str());
      nbwrite (fd, write_buf);
      string in = write_buf;
      string type = in.substr(0, in.find(' '));

      if (type == "jobconf")  {
        job = new Job();
      
      } else if (type == "complete")
        job->status = COMPLETE;

    
    }

};

