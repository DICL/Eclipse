#include <scheduler.hh>
#include <hippomocks.h>
#include <UnitTest++.h>

int connect_mock (int sock, const struct sockaddr* a, socklen_t l) {
 printf ("CONNECT called\n");
 return 0;
}

const char * network [6] = { 
 "10.20.15.160",
 "10.20.15.161",
 "10.20.15.162",
 "10.20.15.163",
 "10.20.15.164",
 "10.20.15.165",
};

struct fix_scheduler{
 Scheduler* m;
 MockRepository mock;

 fix_scheduler () {
  //mock.ExpectCallFunc (&sendto) .Return (10); 
  //mock.ExpectCallFunc (&sendto) .Do (connect_mock);
  m = new Scheduler ();
 }

 ~fix_scheduler () {
  delete m;
 }
};

SUITE (SCHEDULER) {

	// --------------------------------------------------------
	
 TEST_FIXTURE (fix_scheduler, setters) {
   m->set_port (10000);
   m->set_network (6, network);
   
   CHECK_EQUAL (m->get_port(), 10000);
   //CHECK_EQUAL (m->get_nslaves(), 40);
 }

	//// --------------------------------------------------------
 //TEST_FIXTURE (fix_scheduler, ) {
 //}

	//// --------------------------------------------------------
 //TEST_FIXTURE (fix_scheduler, ) {
 //}
}
