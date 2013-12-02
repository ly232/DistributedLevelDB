//common.h
//common headers and macros for client and server
#ifndef _common_h
#define _common_h
//headers:
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h> //socketaddr, ...
#include <netinet/in.h> //sockaddr_in
#include <arpa/inet.h> //inet_pton, htons, ...
#include <netdb.h> //gethostbyname
#include <ifaddrs.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <iostream>
#include <pthread.h>
#include <algorithm>
#include <vector>
#include <map>
#include <queue>
#include <list>
#include <ctime>
//constants:
#define BUF_SIZE 99999
#define MAX_CONN 5
#define MAX_CLUSTER 2 //max number of leveldb server clusters
                      //a cluster is a set of leveldb servers
                      //a tuple (k,v) belongs to cluster hash(k) only
                      //note that hash(k) must be 
                      //in range {0...MAX_CLUSTER-1}
                      //IMPORTANT: each cluster must have at least one
                      //           leveldb server. i.e. there must be
                      //           at least MAX_CLUSTER leveldb servers
                      //           available at any time.
//error code:
#define SOCKET_CONSTRUCT_ERROR 1;
#define SOCKET_BIND_ERROR 2;
#define BAD_INPUT 3;
#define SOCKET_LISTEN_ERROR 4;
#define SOCKET_ACCEPT_ERROR 5;
#define SOCKET_RECEIVE_ERROR 6;
#define FILE_IO_ERROR 7;
#define SOCKET_CLOSE_ERROR 8;
#define SOCKET_ACK_ERROR 9;
#define SOCKET_SEND_ERROR 10;
#define THREAD_ERROR 11;
#define DB_FAIL 12;
#define CLUSTER_FAIL 13;
//utility apis:
inline size_t hash(std::string& s)
{
  size_t len = s.length();
  size_t val = 0;
  for (int i=0; i<len; i++)
    val += (size_t)s[i];
  val %= MAX_CLUSTER;
  return val;
}
#endif
