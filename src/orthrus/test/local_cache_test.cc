#include <simring.hh>
#include <local_cache.hh>
#include <UnitTest++.h>

struct fix_local_cache {
 uint64_t success, failed;
 Local_cache* cache;

 fix_local_cache () {
  success = 0;
  failed = 0;
  cache = new Local_cache (3, "input1.trash");
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
  char data [4098];
  bzero (data, 4098);
  strcpy (data, "WHASAPGUYS");
  disk_page_t dp .set_index (50) .set_size (size) .set_data (data);

  boundaries_update (100, 200);
  insert (50, dp);
 }

 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, lookup) {
 }
}
