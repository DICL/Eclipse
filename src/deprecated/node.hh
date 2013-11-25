/*
 * @file This file contains the source code of the application 
 *       which will run in each server 
 */
#pragma once
#ifndef __NODE_HH_
#define __NODE_HH_

#include <iostream>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <error.h>
#include <simring.hh>

using namespace std;

extern bool panic;
extern SETcache cache; 

extern uint32_t queryRecieves;
extern uint32_t queryProcessed;
extern uint64_t hitCount;
extern uint64_t missCount;
extern uint64_t TotalExecTime;
extern uint64_t TotalWaitTime;

extern int sock_scheduler, sock_left, sock_right, sock_server;  

extern pthread_cond_t cond_scheduler_empty;
extern pthread_cond_t cond_scheduler_full;
extern pthread_cond_t cond_neighbor_empty;
extern pthread_mutex_t mutex_scheduler;
extern pthread_mutex_t mutex_neighbor;

extern ssize_t (*_recv)     (int, void*, size_t, int);
extern ssize_t (*_send)     (int, const void*, size_t, int);
extern int (*_connect)      (int, const struct sockaddr*, socklen_t);

#ifdef _DEBUG
ssize_t recv_mock           (int, void*, size_t, int);
ssize_t sendto_mock         (int, const void*, size_t, int);
ssize_t recvfrom_mock       (int, const void*, size_t, int);
ssize_t send_mock           (int, const void*, size_t, int);
int connect_mock            (int, const struct sockaddr*, socklen_t);
void parse_args             (int, const char**);
#endif

struct Arguments {
  char host_str [32];
  char data_file [256];
  char peer_right [32];
  char peer_left [32];
  int port;
}; 

void* thread_func_dht       (void*) WEAK;
void* thread_func_scheduler (void*) WEAK;
void* thread_func_neighbor  (void*) WEAK;
void* thread_func_forward   (void*) WEAK;

void setup_server_peer      (int, int*, struct sockaddr_in*) WEAK;
void setup_client_peer      (const int, const char*, int*, struct sockaddr_in*) WEAK;
void setup_client_scheduler (int, const char*, int*) WEAK;
void parse_args             (int, const char**, Arguments*) WEAK;
void close_all              (void) WEAK;
void catch_signal           (int);
void recv_msg2              (int fd, char* in);

#endif
