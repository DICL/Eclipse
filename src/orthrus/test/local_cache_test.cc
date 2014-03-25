#include <local_cache.hh>
#include <UnitTest++.h>

const size_t SIZE = 10;

struct fix_local_cache : public Local_cache {
 fix_local_cache () {
  set_policy (orthrus::SPATIAL | orthrus::LRU) .set_size (SIZE);
 }
};

SUITE (LOCAL_CACHE_BASIC) {
 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, cstrdstr) {
  CHECK (get_size () == 10);
  CHECK (get_current_size () == 0);
  CHECK (get_policy () == (orthrus::SPATIAL | orthrus::LRU));
 }
 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, insert) {
  const uint64_t left = 100, right = 200, size = 10;
  disk_page_t dp = disk_page_t() .set_index (50) .set_size (size) .set_data ("WHASAPGUYS");

  boundaries_update (left, right);
  insert (50, dp);
  CHECK (get_current_size () == 10);
 }
 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, lookup) {
  const uint64_t left = 100, right = 200, size = 10;
  disk_page_t dp = disk_page_t() .set_index (50) .set_size (size) .set_data ("WHASAPGUYS");
  boundaries_update (left, right);
  insert (50, dp);
  CHECK (get_current_size () == 10);
  CHECK (lookup (50).get_index() == 50);
 }

 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, is_disk_page_belonging) {
  const uint64_t left = 100, right = 200;
  boundaries_update (left, right);
  CHECK (is_disk_page_belonging (disk_page_t().set_index (150)) == true);
  CHECK (is_disk_page_belonging (disk_page_t().set_index (50)) == false);
  CHECK (get_boundaries().first  == 100);
  CHECK (get_boundaries().second == 200);
 }

 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, get_local_center) {
  const uint64_t left = 100, right = 200;
  boundaries_update (left, right);
  set_size (20);
  insert (120, disk_page_t().set_index (120) .set_size (10));
  insert (180, disk_page_t().set_index (180) .set_size (10));
  CHECK (get_local_center() == 1500);
 }

 //--------------------------------------//
 TEST_FIXTURE (fix_local_cache, pop_farthest) {
  const uint64_t left = 100, right = 200;
  boundaries_update (left, right);
  set_size (20);
  insert (120, disk_page_t().set_index (130) .set_size (10));
  insert (180, disk_page_t().set_index (190) .set_size (10));
  CHECK (get_local_center() == 1600);
  insert (180, disk_page_t().set_index (130) .set_size (10)); // Farthest was poped out
  CHECK (get_local_center() == 1300);
 }
}
