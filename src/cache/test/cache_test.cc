#include <cache.hh>
#include <UnitTest++.h>
//
// --------UNIT TEST AUTOMATICALLY GENERATED------------
//
struct fix_cache {
 Cache* victim;

 fix_cache () {
  victim = new Cache ();
 }
 ~fix_cache () {
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

 TEST(parallel) { 
#pragma omp parallel sections
  {
   const char * network [2] = {"127.0.0.1", "127.0.0.2"};
   omp_lock_t lock, lock2;
   omp_init_lock (&lock);
   omp_init_lock (&lock2);
   omp_set_lock (&lock);         //! Set Lock 1 to 1
   omp_set_lock (&lock2);        //! Set lock 2 to 1

#pragma omp section
   {
    //First Node
    Cache cache (2);
    cache.set_network (network);
    cache.set_policy (CACHE_LRU);
    cache.bind ();
    cache.run ();

    omp_unset_lock (&lock2);       //! Wait till the other thread finish

    cache.insert (10, "test1");
    cache.insert (20, "test2");
    cache.insert (30, "test3");

    cache.close ();
   }
#pragma omp section
   {
    //First Node
    Cache cache (2);
    cache.set_network (network);
    cache.set_policy (CACHE_LRU);
    cache.bind ();
    cache.run ();

    omp_set_lock (&lock2);       //! Wait till the other thread finish
    CHECK(cache.insert (10, "test1") == true);  //! It test if the dp was migrated

    cache.close ();
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
