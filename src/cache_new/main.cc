#include "cache_slave.hh"

int main (void) {
  Cache_slave cache_slave;

  cache_slave.connect();
  cache_slave.main_loop();

  return EXIT_SUCCESS;
}
