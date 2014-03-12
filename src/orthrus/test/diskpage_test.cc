#include <diskpage.hh>
#include <UnitTest++.h>
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
const char str_head [1024] = 
 "INDEX:00000000000000000000000000000100:"
 "TIME:00000000000000000000000000000005:"
 "SIZE:00000000000000000000000000000010";

const char str_data [1024] = "WASSUPGUYS";

struct fix_diskpage_test {
 disk_page_t* victim;
 char str_full [2048];

 fix_diskpage_test () {
  victim = new disk_page_t ();
  memcpy (str_full, str_head, 1024);
  memcpy (str_full + 1023, str_data, 1024);
 }
 ~fix_diskpage_test () {
  delete victim;
 }
};
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//

SUITE (DISKPAGE_TEST) {
 // ----------------------------------------------------
 TEST_FIXTURE (fix_diskpage_test, ctor_dtor) { }
 TEST_FIXTURE (fix_diskpage_test, serialize) {
  (*victim)
   .set_index (100)
   .set_size (10)
   .set_time (5)
   .set_data ("WASSUPGUYS");

  const char* attemp = victim->serialize();
  CHECK_EQUAL (attemp, str_full);
  delete attemp;
 }
 TEST_FIXTURE (fix_diskpage_test, deserialize) {
  (*victim).deserialize (str_full);

  CHECK_EQUAL (victim->get_index(), 100);
  CHECK_EQUAL (victim->get_size(), 10);
  CHECK_EQUAL (victim->get_time(), 5);
 }
}
// -----------------------------------------------------
