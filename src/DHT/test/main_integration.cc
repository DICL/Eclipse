#include <DHTclient.hh>
#include <DHTserver.hh>
#include <UnitTest++.h>
#include <stdio.h>
#include <TestReporterStdout.h>
#include <omp.h>
#include <string.h>

using namespace UnitTest;

#define PORT 10000

struct functor_test_A 
{
 bool operator () (const UnitTest::Test* _test) const {
  if (strcmp (_test->m_details.testName, "SERVER_MAIN"))
   return true;
  return false;
 }
};

struct functor_test_B
{
 bool operator () (const UnitTest::Test* _test) const {
  if (strcmp (_test->m_details.testName, "CLIENT_MAIN")) {
   return true;
  }

  return false;
 };
}; 

int main () {
 omp_lock_t lock, lock2;
 omp_init_lock(&lock);
 omp_init_lock(&lock2);
 omp_set_lock(&lock); // Wait till the other thread finish
 omp_set_lock(&lock2); // Wait till the other thread finish

#pragma omp parallel sections
 {
#pragma omp section
  {
   DHTserver server (PORT);

   server.bind ();

   server.listen ();
   server.report ("Ahoy", 0);
   server.report ("hola", 1);
   server.report ("adios", 2);

   omp_unset_lock(&lock2);
   omp_set_lock(&lock); // Wait till the other thread finish
   server.close();
  }

#pragma omp section
  {
   omp_set_lock(&lock2); // Wait till the other thread finish
   DHTclient client ("10.20.15.170", PORT);

   client.bind ();

   if (0 != client.lookup ("Ahoy"))
    printf("TEST1, failed\n");
   else 
    printf("TEST1, success\n");

   if (1 != client.lookup ("hola"))
    printf("TEST2, failed\n");
   else 
    printf("TEST2, success\n");

   if (2 != client.lookup ("adios"))
    printf("TEST3, failed\n");
   else 
    printf("TEST3, success\n");

   client.close();
   omp_unset_lock(&lock);
  }
 }
}
