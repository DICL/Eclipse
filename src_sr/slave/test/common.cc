#define _DEBUG
#include <node.hh>

static int c = 0;
int connect_mock (int sock, const struct sockaddr* a, socklen_t l) {
 printf ("CONNECT called\n");
 return 0;
}

void parse_args (int argc, const char** argv) {
	strcpy (host_str, "HOST");
	strcpy (peer_right, "right_server");
	strcpy (peer_left, "left_server");
	strcpy (data_file, "/scratch/bsnam/garbage1000.dat");
}

void recv_msg (int fd, char* in) {
 sleep (1);
 if (c++ < 10) {
	 strcpy (in, "QUERY");
 } else if (c++ == 11) {
	 strcpy (in, "INFO");
 } else {
	 strcpy (in, "QUIT");
 }
}
void recv_msg2 (int fd, char* in) {
   sleep (1);
	 strcpy (in, "QUERY");
}
