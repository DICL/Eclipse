#include <DHTserver.hh>

int main(int argc, const char *argv[])
{
 DHTserver server (5555);
 
 server.bind ();
 server.set_interface ("eth0");
 server.listen ();
 
 server.report ("Ahoy", 0);
 server.report ("hola", 1);
 server.report ("adios", 2);

 server.close();
 return 0;
}
