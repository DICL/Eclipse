#include "../common/ecfs.hh"
#include "fileserver.hh"

#include <iostream>
#include <exception>

int main (int argc, const char *argv[]) {
  using namespace std;

  char* master_address;
  int dhtport;
  fileserver afileserver;

  try {
    Settings setted;
    setted.load();
    dhtport        = setted.get<int> ("network.port_mapreduce");
    master_address = strndup (setted.get<string>("network.master").c_str(), BUF_SIZE);

  } catch (exception& e) {
    cerr << e.what() << endl;
  }

  afileserver.run_server (dhtport, master_address); //! run the file server
  return EXIT_SUCCESS;
}
