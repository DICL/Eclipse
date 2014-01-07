#include <UnitTest++.h>
#include <DHTclient.h>

TEST (CLIENT_MAIN) {
 DHTclient client ("localhost", 5555);

 client.bind ();

 CHECK (0 == client.lookup ("Ahoy"))
 CHECK (1 == client.lookup ("hola"))
 CHECK (2 == client.lookup ("adios"))

 client.close();
 return 0;
}
