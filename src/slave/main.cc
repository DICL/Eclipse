#include "slave.hh"
#include "../cache_slave/cache_slave.hh"
#include <thread>

int main (int argc, char** argv) {
  Slave slave;

  auto cache_thread = std::thread ([&] () {
      sleep (5);
      Cache_slave cache_slave;
      cache_slave.connect ();
      cache_slave.run_server (); 
      });

  slave.main_loop ();

  cache_thread.join();

  return EXIT_SUCCESS;
}
