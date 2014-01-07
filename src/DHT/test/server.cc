#include <UnitTest++.h>
#include <DHTserver.hh>

TEST (SERVER_MAIN) {
 DHTserver server (5555);
 
 server.bind ();
 server.set_interface ("eth0");
 server.listen ();
 
 server.report ("Ahoy", 0);
 server.report ("hola", 1);
 server.report ("adios", 2);

 sleep (5);
 server.close();
 return 0;
}
