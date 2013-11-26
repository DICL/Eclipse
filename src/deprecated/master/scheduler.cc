#include <signal.h>
#include <node_client.hh>
#include <simring.hh>
#include <err.h>
#include <setjmp.h>

#define RULER()                                        \
   printf ("%-80.145s\n", (                            \
   "---------------------------------------------"     \
   "---------------------------------------------"     \
   "---------------------------------------------"     \
   "---------------------------------------------"     \
   "---------------------------------------------")) 
    

using namespace std;

int sock, port = 0, nservers = 0;
int16_t* connected;
uint64_t TotalCacheHit = 0, TotalCacheMiss = 0, numQuery = 0;
uint64_t TotalExecTime = 0, TotalWaitTime = 0, shiftedQuery = 0, SentShiftedQuery = 0; 
uint64_t AveExecTime = 0, AveWaitTime = 0; 
uint64_t MaxExecTime = 0, MaxWaitTime = 0; 
uint64_t RequestedData = 0, ReceivedData = 0; 
static jmp_buf finish;
Node** backend;

void receive_all (void) {
	char recv_data [LOT];

	TotalCacheHit = 0, TotalCacheMiss = 0,  numQuery = 0;
 TotalExecTime = 0, TotalWaitTime = 0; 
 AveExecTime = 0, AveWaitTime = 0; 
 MaxExecTime = 0, MaxWaitTime = 0; 
 RequestedData = 0, ReceivedData = 0; 
 SentShiftedQuery = 0, shiftedQuery = 0;

	for (int i = 0; i < nservers; i++) backend[i]->send_msg ("INFO");  
	for (int i = 0; i < nservers; i++) {  

		bzero (recv_data, LOT);
		recv (backend[i]->get_fd (), recv_data, LOT, MSG_WAITALL);

		for (char *key= strtok(recv_data, "=\n"); key != NULL; key= strtok(NULL, "=\n")) { 
			char* val = strtok (NULL, "=\n");

			if (strcmp (key, "CacheHit") == 0)
				TotalCacheHit += strtoul (val, NULL, 10);

			else if (strcmp (key, "CacheMiss") == 0)
				TotalCacheMiss += strtoul (val, NULL, 10);

			else if (strcmp (key, "QueryCount") == 0)
				numQuery += strtoul (val, NULL, 10);

			else if (strcmp (key, "TotalExecTime") == 0) {
				TotalExecTime += strtoull (val, NULL, 10);

				if (strtoull (val, NULL, 0) > MaxExecTime)
					MaxExecTime = strtoull (val, NULL, 10);
			}
			else if (strcmp(key, "TotalWaitTime") == 0) {
				TotalWaitTime += strtoull (val, NULL, 10);

				if (strtoull (val, NULL, 0) > MaxWaitTime)
					MaxWaitTime = strtoull (val, NULL, 10);
			}

			else if (strcmp(key, "shiftedQuery") == 0) {
				shiftedQuery += strtoull (val, NULL, 10);
      }
			else if (strcmp(key, "SentShiftedQuery") == 0) {
				SentShiftedQuery += strtoull (val, NULL, 10);
      }
			else if (strcmp(key, "RequestedData") == 0) {
				RequestedData += strtoull (val, NULL, 10);
      }
			else if (strcmp(key, "ReceivedData") == 0) {
				ReceivedData += strtoull (val, NULL, 10);
      }
		}
    AveExecTime = TotalExecTime / nservers;
    AveWaitTime = TotalWaitTime / nservers;
	}
}

/*
 * 
 */
void print_header (void) {
 RULER ();
 printf (
   "|%15.15s|%15.15s|%15.15s|%15.15s|%15.15s|%15.15s|" 
   "%15.15s|%15.15s|%15.15s|\n", 

   "Queries", "Hits", "Miss", "recvShiftedQuery", "SentShiftedQuery",
   "RequestedData", "ReceivedData", "AveExecTime", "AveWaitTime"
 );
 RULER ();
}

/*
 * 
 */
void print_out (void) {
 printf (
   "|%15" PRIu64 "|%15" PRIu64 "|%15" PRIu64 "|%15" PRIu64 "|%15" PRIu64
   "|%15" PRIu64 "|%15" PRIu64 "|%15.5LE|%15.5LE|\n",

    numQuery, TotalCacheHit, TotalCacheMiss, shiftedQuery, SentShiftedQuery,
    RequestedData, ReceivedData,
   ((long double)AveExecTime)   / 1000000.0,
   ((long double)AveWaitTime)   / 1000000.0
 );
}

/*
 * 
 */
void wakeUpServer (void) {
 int one = 1;
 struct sockaddr_in addr;

 addr.sin_family = AF_INET;
 addr.sin_port = htons (port);
 addr.sin_addr.s_addr = INADDR_ANY;
 bzero (&(addr.sin_zero),8);

 if ((sock = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  err (EXIT_FAILURE, "[SCHEDULER] Socket");

 if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1)
  err (EXIT_FAILURE, "[SCHEDULER] Setsockopt");

 if (bind (sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1)
  err (EXIT_FAILURE, "[SCHEDULER] Unable to bind");

 if (listen (sock, nservers + 1) == -1)
  err (EXIT_FAILURE, "[SCHEDULER] Listen");

 log (M_INFO, "SCHEDULER", "Network setted up using port = %i", port);
}

/*
 * 
 */
void catchSignal (int Signal) {
 log (M_INFO, "SCHEDULER", "Closing sockets & files. REASON: %s: ", strsignal(Signal));
 longjmp (finish, 1);
 close (sock);
 exit (EXIT_FAILURE);
}


int main (int argc, char** argv) {
 uint32_t nqueries = 0, step = 10000;
 uint32_t cnt = 0;
 const uint64_t data_interval = 1000000;
 struct timeval start, end;

 int c;
 while ((c = getopt (argc, const_cast<char**> (argv), "q:n:p:s:")) != -1)
  switch (c) {
   case 'q': nqueries = atoi (optarg); break;
   case 'n': nservers = atoi (optarg); break;
   case 'p': port = atoi (optarg);     break;
   case 's': step = atoi (optarg);     break;
  }

 if (!nqueries || !nservers || !port)
   log (M_ERR, "SCHEDULER", "PARSER: all the options needs to be setted");
 
 backend = new Node* [nservers];
 signal (SIGINT | SIGSEGV | SIGTERM, catchSignal);		//catching signals to close sockets and files

 wakeUpServer();

 for (int i = 0; i < nservers; i++) (backend[i] = new Node ())->accept (sock);

 log (M_INFO, "SCHEDULER", "All backend servers linked");
 print_header ();

 if (setjmp (finish)) goto end; 
 
 //! Initiliaze the node equally
 { 
   int j = 0;
   for (uint64_t i = 0; i < data_interval; i += (data_interval/nservers)) {
     backend [j++]->set_alpha (ALPHA).set_EMA (i + (data_interval/nservers)/2);
   }
 }  

 gettimeofday (&start, NULL);
 //! Main loop which use the given algorithm to send the queries
 for (char line[100]; fgets(line, 100, stdin) != NULL && cnt < nqueries; cnt++)
 {
  int selected = 0;
  uint64_t point = prepare_input (line);
  double minDist = DBL_MAX;
  
  //usleep (50);

  for (int i = 0; i < nservers; i++) {
   if (backend[i]->get_distance (point) < minDist) {
    selected = i;
    minDist = backend[i]->get_distance (point);
   }
  }
  Node& victim = *(backend [selected]);

  //! :TODO: Circular boundaries
  if (selected == 0) {                     //! For now lets fix the first node low boundary in 0
    victim.set_low (.0);

  } else {
    victim.set_low (victim.get_EMA() - ((victim.get_EMA () - backend [(selected - 1)]->get_EMA ()) / 2.0));
  } 

  if (selected == nservers - 1 || backend[selected + 1] == NULL) {
    victim.set_upp (DBL_MAX);  //! For now lets fix the last node upp boundary in the maximum num

  } else {
    victim.set_upp (victim.get_EMA() + ((backend [selected + 1]->get_EMA () - victim.get_EMA ()) / 2.0));
  }

  victim .update_EMA (point) .set_time (cnt);
  if (cnt % 2000 == 0) {
    victim.send (point, true);

  } else {
    victim.send (point);
  }

  if ((cnt + 1) % step == 0) {
   sleep (1);
   receive_all ();
   print_out ();
  }
 }
 RULER ();

 //! Delete dynamic objects and say bye to back-end severs
 for (int i = 0; i < nservers; i++) backend[i]->send_msg ("QUIT") .close ();  

 gettimeofday (&end, NULL);
 printf ("SchedulerTime:\t %20LE S\n", (long double) timediff (&end, &start) / 1000000.0);


end:

 //! Close sockets
 close (sock);
 for (int i = 0; i < nservers; i++) backend[i]->close ();
 for (int i = 0; i < nservers; i++) delete backend[i];
 delete[] backend;

 return EXIT_SUCCESS;
}
