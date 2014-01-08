#define _DEBUG

#include <DHTclient.hh>
#include <DHTserver.hh>
#include <UnitTest++.h>
#include <stdio.h>
#include <TestReporterStdout.h>
#include <omp.h>
#include <string.h>

int main(int argc, const char *argv[]) {
 DHTclient client ("127.0.0.1", 9999);

 client.bind ();

 sleep (1); // Give sometime to get ready
 if (0 != client.lookup ("Ahoy"))
  perror ("Error");

 sleep (1); // Give sometime to get ready
 if (1 != client.lookup ("hola"))
  perror ("Error");

 sleep (1); // Give sometime to get ready
 if (2 != client.lookup ("adios"))
  perror ("Error");

 client.close();
 return 0;
}
