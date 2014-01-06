#include <DHTclient.hh>

int main(int argc, const char *argv[])
{
 DHTclient client ("localhost", 5555);

 client.bind ();

 if (0 != client.lookup ("Ahoy"))
  err ("bad");
 if (1 != client.lookup ("hola"))
  err ("bad");
 if (2 != client.lookup ("adios"))
  err ("bad");

 client.close();
 return 0;
}
