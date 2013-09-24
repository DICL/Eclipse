#include <master.hh>
#include <UnitTest++.h>

struct address_book_test_t {
 Address_book* a; 

 address_book_test_t () {
  a = new Address_book ();
  a->set_EMA (10f);
  a->set_alpha (0.03f);
 }

 ~address_book_test_t () {
  delete a;
 }
};

SUITE (address_book_test) {

 // --------------------------------------------------- 
 TEST_FIXTURE (address_book_test_t, constructor) {
 }

 // --------------------------------------------------- 
 TEST_FIXTURE (address_book_test_t, get_distance) {
  CHECK_EQUAL (a->get_distance (20f), 10f);
 }
}
