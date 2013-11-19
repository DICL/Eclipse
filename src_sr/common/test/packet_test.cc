#include <packet.hh>
#include <UnitTest++.h>
//
// -----------------------------------------------------
//
struct fix_Packet {
 Packet* victim;

 fix_Packet () {
  victim = new Packet (new File ("file.txt"));
 }
 ~fix_Packet () {
  delete victim;
 }
};
//
// -----------------------------------------------------
//
SUITE (Packet) {
 // ----------------------------------------------------
 TEST_FIXTURE (fix_Packet, ctor_dtor) { }
}
// -----------------------------------------------------
