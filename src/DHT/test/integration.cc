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
   server.report ("Ahoy", 0);
   server.report ("hola", 1);
   server.report ("adios", 2);

   omp_unset_lock (&lock2);    //! NOW thread 2 can start executing
   omp_set_lock (&lock);       //! Wait till the other thread finish
   server.close ();
  }

#pragma omp section
  {
   omp_set_lock (&lock2);       //! Wait till the other thread finish
   DHTclient client (ADDRESS, PORT);

   client.bind ();

   CHECK (0 == client.lookup ("Ahoy"));
   CHECK (1 == client.lookup ("hola"));
   CHECK (2 == client.lookup ("adios"));

   client.close();
   omp_unset_lock (&lock);
  }
  // I even got a headache writing this tests, but I had real fun, due to that stress I 
  // had a argument with my girlfriend hehe 
 }
}
