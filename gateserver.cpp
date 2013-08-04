//gateserver.cpp
#include "include/gateserver.h"
#include "include/syncobj.h"

void* gateserver::main_thread(void* arg)
{
  /*
   * main thread handles a client request by 3 phases:
   * 1) spawn a recv_thread to read the entire json request
   * 2) identify a leveldb server and send request to that server
   * 3) wait for leveldb server's response,
   *    then pass that response to client
   */

  int clfd = *(int*)arg;

  std::string* ackmsg = new std::string;
  *ackmsg = "ack from gateway";
  std::string* leveldbrsp = new std::string; 
               //recv thread will fill in leveldbrsp

  //gateserver::syncobj* so = new gateserver::syncobj;
  syncobj* so = new syncobj(2L, 2L, 1L);

  std::vector<void*> thread_arg;
  thread_arg.push_back((void*)&clfd);
  thread_arg.push_back((void*)so);
  thread_arg.push_back((void*)ackmsg);
  thread_arg.push_back((void*)leveldbrsp);

  //for recv thread:
  if (pthread_create(&so->_thread_obj_arr[0], 
		     0, 
		     &recv_thread, 
		     (void*)&thread_arg)
      !=0)
    throw THREAD_ERROR;

  if (pthread_join(so->_thread_obj_arr[0], 0)!=0)
    throw THREAD_ERROR;

  //for send thread:
  if (pthread_create(&so->_thread_obj_arr[1], 
		     0, 
		     &send_thread, 
		     (void*)&thread_arg)
      !=0)
    throw THREAD_ERROR;


  if (pthread_join(so->_thread_obj_arr[1], 0)!=0)
    throw THREAD_ERROR;

  //delete socket_mutex;
  delete (int*)arg;
  delete ackmsg;
  delete so;

  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
  return 0;
}

//requestHandler does not block
//it passes the work down to another thread main_thread
//main_thread will block, but that does not block gateway server
void gateserver::requestHandler(int clfd)
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

void* gateserver::send_thread(void* arg)
{
  std::vector<void*> argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t socket_mutex
    = ((syncobj*)argv[1])->_mutex_arr[0];
  std::string resp_str = (*(std::string*)argv[2]);
  const char* resp = resp_str.c_str();
  size_t resp_len = strlen(resp)+1;
  int tmp;
  pthread_mutex_lock(&socket_mutex);
  if ((tmp=write(clfd, resp, resp_len))!=resp_len)
  {
    throw FILE_IO_ERROR;
  }
  pthread_mutex_unlock(&socket_mutex);
  return 0;
}

void* gateserver::recv_thread(void* arg)
{
  std::vector<void*> argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t socket_mutex 
    = ((syncobj*)argv[1])->_mutex_arr[0];
  int byte_received = -1;
  std::string request;
  char buf[BUF_SIZE];
  bool done = false;
  while (!done)
  {
    memset(buf, 0, BUF_SIZE);
    pthread_mutex_lock(&socket_mutex);
    byte_received=read(clfd, buf, BUF_SIZE);
    pthread_mutex_unlock(&socket_mutex);
    if (byte_received < BUF_SIZE)
    {
      buf[byte_received] = '\0';
      done = true;
    }
    request = request + std::string(buf);
  }
  std::cout<<"request="<<request<<std::endl;
  return 0;
}
