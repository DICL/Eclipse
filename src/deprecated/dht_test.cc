#include <dht.hh>
#include <UnitTest++.h>
#include <iostream>

using namespace std;

const char * ips [32] = {
 "192.168.0.1",
 "192.168.0.192",
 "192.168.0.5",
 "192.168.0.9"
};

SUITE (DHT) {
 TEST (ALL) {
  DHT my_dht;
  Header h1;

  my_dht.set_network (24444, 4, "ra0", ips); 
  h1.set_point (320000);// .set_trazable ();

  CHECK_EQUAL(my_dht.check (h1), 0) ;

  if (my_dht.check (h1) == false) {
   CHECK_EQUAL (my_dht.request (h1), 1);
  }
 }
}
