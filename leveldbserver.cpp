//leveldbserver.cpp
#include "include/leveldbserver.h"
//#include <include/db.h>
void leveldbserver::requestHandler(int clfd)
{
  pthread_t main_thread_obj;
  int* clfdptr = new int;
  *clfdptr = clfd;
  if(pthread_create(&main_thread_obj, 
		 0, 
		 &main_thread, 
		 (void*)clfdptr)
     !=0)
    throw THREAD_ERROR;
}

void* main_thread(void*)
{
  
  return 0;
}
void* send_thread(void*)
{
  return 0;
}
void* recv_thread(void*)
{
  return 0;
}