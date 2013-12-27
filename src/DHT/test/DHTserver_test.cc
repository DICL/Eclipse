#include <DHTserver.hh>
#include <UnitTest++.h>
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
struct fix_DHTserver {
 DHTserver* victim;

 fix_DHTserver () {
  victim = new DHTserver ();
 }
 ~fix_DHTserver () {
  delete victim;
 }
};
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
SUITE (DHTSERVER_TEST) {
 // ----------------------------------------------------
 TEST_FIXTURE (fix_DHTserver, ctor_dtor) { }
}
// -----------------------------------------------------
