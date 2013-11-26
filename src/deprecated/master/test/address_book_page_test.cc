#include <address_book_page.hh>
#include <UnitTest++.h>

struct address_book_page_test_t {
 Address_book_page *a;
 int server_sock;

 address_book_page_test_t () {
  a = new Address_book_page ("node1", 2323); 
 }

 ~address_book_page_test_t () {
  delete a;
 }
}

SUITE (address_book_page_test) {

 // --------------------------------------------------- 
 TEST_FIXTURE (address_book_page_test_t, cstor_dstor) { }

 // --------------------------------------------------- 
 TEST_FIXTURE (address_book_page_test_t, misc) {
  a->set_fd (server_sock);
  CHECK_EQUAL (a->get_fd (), server_sock);
  CHECK_EQUAL (a->get_status (), DISCONNECTED)
 }
}
