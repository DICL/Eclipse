#include <simring.hh>
#include <SETcache.hh>
#include <UnitTest++.h>

struct fix_setcache {
 uint64_t success, failed;
 SETcache* cache;

 fix_setcache () {
  success = 0;
  failed = 0;
  cache = new SETcache (3, "input1.trash");
 }

 ~fix_setcache () {
  delete cache;
 }
};

SUITE (SETCACHE) {
 //--------------------------------------//
 TEST_FIXTURE (fix_setcache, match) {

  for (int i = 3; i < 7; i++) {

   if (cache->match (i, 1, 10)) 
    success++;
   else
    failed++;

  }
  CHECK(failed == 4);
 }
}
