#include <DHTclient.hh>
#include <UnitTest++.h>
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
struct fix_DHTclient {
 DHTclient* victim;

 fix_DHTclient () {
  victim = new DHTclient ("localhost", 5000);
 }
 ~fix_DHTclient () {
  delete victim;
 }
};
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
SUITE (DHTCLIENT_TEST) {
 // ----------------------------------------------------
 TEST_FIXTURE (fix_DHTclient, ctor_dtor) { }

 // ----------------------------------------------------
 TEST_FIXTURE (fix_DHTclient, setup) { 
  CHECK (victim->bind () == true);
  CHECK (victim->close () == true);
 }
}
// -----------------------------------------------------
