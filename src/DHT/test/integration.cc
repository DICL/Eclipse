#include <DHTclient.hh>
#include <DHTserver.hh>
#include <UnitTest++.h>
#include <stdio.h>
#include <omp.h>
#include <string.h>

#define ADDRESS "127.0.0.1"
#define PORT 10000

TEST (INTEGRATION) {
 omp_lock_t lock, lock2;
 omp_init_lock (&lock);
 omp_init_lock (&lock2);
 omp_set_lock (&lock);         //! Set Lock 1 to 1
 omp_set_lock (&lock2);        //! Set lock 2 to 1

#pragma omp parallel sections
 {
#pragma omp section
  {
   DHTserver server (PORT);

   server.bind ();

   server.listen ();
   server.report ("Ahoy",  "10.0.0.1");
   server.report ("hola",  "10.0.0.2");
   server.report ("adios", "10.0.0.3");

   omp_unset_lock (&lock2);    //! NOW thread 2 can start executing
   omp_set_lock (&lock);       //! Wait till the other thread finish
   server.close ();
  }

#pragma omp section
  {
   omp_set_lock (&lock2);       //! Wait till the other thread finish
   //UNITTEST_TIME_CONSTRAINT (2000);
   DHTclient client (ADDRESS, PORT);

   client.bind ();

   CHECK_EQUAL (client.lookup_str ("Ahoy"),  "10.0.0.1");
   CHECK_EQUAL (client.lookup_str ("hola"),  "10.0.0.2");
   CHECK_EQUAL (client.lookup_str ("adios"), "10.0.0.3");

   CHECK (-1 == client.lookup ("arrivederci"));                     //! Not found 
   CHECK_EQUAL (client.lookup_str ("jol"),         "NOTFOUND");     //! Not found 
   CHECK_EQUAL (client.lookup_str ("arrivederci"), "NOTFOUND");     //! Not found 

   client.close();
   omp_unset_lock (&lock);
  }
 }
}
