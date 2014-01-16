#include <cache.hh>
#include <UnitTest++.h>
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
struct fix_cache_test {
 cache_test* victim;

 fix_cache_test () {
  victim = new cache_test ();
 }
 ~fix_cache_test () {
  delete victim;
 }
};
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
SUITE (CACHE_TEST) {
 // ----------------------------------------------------
 TEST_FIXTURE (fix_cache_test, ctor_dtor) { }

 // ----------------------------------------------------
 TEST_FIXTURE (fix_cache_test, normal) { 
   victim->set_maxsize (4);
   victim->set_policy (CACHE_LRU);
   
   victim->insert (0, "Hello");
   victim->insert (1, "Hola");
   victim->insert (2, "Allo");
   victim->insert (3, "Annyeong");
   victim->insert (4, "Seno");

   CHECK("Hello",    victim->lookup (0));
   CHECK("Hola",     victim->lookup (1));
   CHECK("Allo",     victim->lookup (2));
   CHECK("Annyeong", victim->lookup (3));
   CHECK("Seno",     victim->lookup (4));
 }

 TEST_FIXTURE (fix_cache_test, parallel) { 
  victim->set_maxsize (4);
  victim->set_policy (CACHE_LRU | CACHE_REENTRANT);
#pragma omp parallel sections
  {
#pragma omp section
   {

   }
#pragma omp section
   {

   }
  }
 }

 TEST_FIXTURE (fix_cache_test, lru) { 
   victim->set_maxsize (3);
   victim->set_policy (CACHE_LRU);
   victim->insert (0, "Hello");
   victim->insert (1, "Hola");
   victim->insert (2, "Allo");
   victim->insert (3, "Annyeong");
   CHECK_THROW (victim->lookup (0), std::out_of_range);

   victim->insert (4, "Seno");

   CHECK("Hello",    victim->lookup (0));
   CHECK("Hola",     victim->lookup (1));
   CHECK("Allo",     victim->lookup (2));
   CHECK("Annyeong", victim->lookup (3));
   CHECK("Seno",     victim->lookup (4));
 }
}
// -----------------------------------------------------
