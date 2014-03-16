#include <local_cache.hh>
#include <UnitTest++.h>

const size_t SIZE = 10;

struct fix_local_cache {
 uint64_t success, failed;
 Local_cache* cache;

 fix_local_cache () {
  success = 0;
  failed = 0;
  cache = new Local_cache ();
  cache .set_policy (CACHE_SPATIAL) .set_size (SIZE);
 }
 ~fix_local_cache () {
  delete cache;
 }
};

SUITE (LOCAL_CACHE) {
 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, cstrdstr) { }
 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, insert) {
  const uint64_t left = 100, right = 200, size = 10;
  disk_page_t dp .set_index (50) .set_size (size) .set_data ("WHASAPGUYS");

  boundaries_update (100, 200);
  insert (50, dp);
 }

 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, lookup) {
 }
}
