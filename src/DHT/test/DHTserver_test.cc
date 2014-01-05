#include <DHTserver.hh>
#include <UnitTest++.h>
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
struct fix_DHTserver {
 DHTserver* victim;

 fix_DHTserver () {
  victim = new DHTserver (5000);
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

 // ----------------------------------------------------
 TEST_FIXTURE (fix_DHTserver, setup) { 
  CHECK (victim->bind () == true);
  CHECK (victim->listen () == true);
  CHECK (victim->close () == true);
 }
}
// -----------------------------------------------------
