#include "../dht.hh"
#include <iostream>

using namespace std;

const char * ips [32] = {
 "192.168.0.1",
 "192.168.0.192",
 "192.168.0.5",
 "192.168.0.9"
};

DHT my_dht;

int main () {
 Header h1;

 my_dht.set_network (24444, 4, "ra0", ips); 
 h1.set_point (320000) .set_trazable ();

 cout << "CHECK: " << my_dht.check (h1) << endl;

 if (my_dht.check (h1) == false) {
  cout << "REQUEST: " << my_dht.request (h1) << endl;
 }
}
