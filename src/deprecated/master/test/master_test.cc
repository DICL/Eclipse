#include <master.hh>
#include <hippomocks.h>
#include <UnitTest++.h>

int connect_mock (int sock, const struct sockaddr* a, socklen_t l) {
 printf ("CONNECT called\n");
 return 0;
}

struct fix_master {
 Master* m;
 MockRepository mock;

 fix_master () {
  mock.ExpectCallFunc (&sendto) .Return (10); 
  mock.ExpectCallFunc (&sendto) .Do (connect_mock);
  m = new Master ();
 }

 ~fix_master () {
  delete m;
 }
};

SUITE (MASTER) {

	// --------------------------------------------------------
	
 TEST_FIXTURE (fix_master, setters) {
   (*m).set_port (10000) 
       .set_nslaves (40)
       .set_signals ();

   CHECK_EQUAL (m->get_port(), 10000);
   CHECK_EQUAL (m->get_nslaves(), 40);
 }

	//// --------------------------------------------------------
 //TEST_FIXTURE (fix_master, ) {
 //}

	//// --------------------------------------------------------
 //TEST_FIXTURE (fix_master, ) {
 //}
}
