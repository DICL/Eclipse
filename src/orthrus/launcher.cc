#include <orthrus.h>

const int NOCLIENTS = 10;
const int PORT      = 10000;
const char* HOST    = "launcher";

char* make_request (char * in) {


}
 
char* make_parcel (char * in) {
 static output 

}

int main(int argc, const char *argv[])
{
 struct sockaddr_in addr;
 int master_fd, fd [NOCLIENT], one = 1;
 Orthrus orthrus (argc, argv);  
 orthrus .bind() .run();
  
 bzero (&(addr.sin_zero), 8);
 addr.sin_family = AF_INET;
 addr.sin_port = htons (port);
 addr.sin_addr.s_addr = htonl (INADDR_ANY);

 if ((master_fd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  log (M_ERR, HOST , "socket function");

 if (setsockopt (master_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  log (M_ERR, HOST, "setsockopt function");

 if (::bind (master_fd, (struct sockaddr*)&addr, sizeof (addr)) == -1)
  log (M_ERR, HOST, "bind function");

 while (true) {
  if (fd_is_ready (master_fd)) {



  }
 }
 
 orthrus.close ();
 return 0;
}
