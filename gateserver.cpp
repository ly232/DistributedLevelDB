//gateserver.cpp
#include "include/gateserver.h"
#include "include/syncobj.h"
#include "include/client.h"

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
  std::string* leveldbrsp = new std::string; 
               //recv thread will fill in leveldbrsp

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

  //for send thread:
  if (pthread_create(&so->_thread_obj_arr[1], 
		     0, 
		     &send_thread, 
		     (void*)&thread_arg)
      !=0)
    throw THREAD_ERROR;

  if (pthread_join(so->_thread_obj_arr[0], 0)!=0) //recv thread
    throw THREAD_ERROR;
  if (pthread_join(so->_thread_obj_arr[1], 0)!=0) //send thread
    throw THREAD_ERROR;

  delete (int*)arg;
  delete ackmsg;
  delete so;
  delete leveldbrsp;

  //fsync(clfd);

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
  int* clfdptr = new int; //will be deleted by main thread
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
  std::vector<void*>& argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t& socket_mutex
    = ((syncobj*)argv[1])->_mutex_arr[0];
  pthread_mutex_t& cv_mutex
    = ((syncobj*)argv[1])->_mutex_arr[1];
  pthread_cond_t& cv = ((syncobj*)argv[1])->_cv_arr[0];
  std::string& resp_str = (*(std::string*)argv[2]);

  //wait until recv thread finishes reception 
  //and get a response from leveldb server
  if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
  while(resp_str.empty())
  {
    pthread_cond_wait(&cv, &cv_mutex);
  }
  const char* resp = resp_str.c_str();
  size_t resp_len = strlen(resp)+1;
  if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;

  if (pthread_mutex_lock(&socket_mutex)!=0) throw THREAD_ERROR;
  if (write(clfd,resp,resp_len)!=resp_len) throw FILE_IO_ERROR;
  if (pthread_mutex_unlock(&socket_mutex)!=0) throw THREAD_ERROR;
  return 0;
}

void* gateserver::recv_thread(void* arg)
{
  std::vector<void*>& argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t& socket_mutex 
    = ((syncobj*)argv[1])->_mutex_arr[0];
  int byte_received = -1;
  std::string request;
  char buf[BUF_SIZE];
  bool done = false;
  pthread_cond_t& cv = ((syncobj*)argv[1])->_cv_arr[0];
  pthread_mutex_t& cv_mutex = ((syncobj*)argv[1])->_mutex_arr[1];
  std::string* ackmsg  = (std::string*)argv[2];
  while (!done)
  {
    memset(buf, 0, BUF_SIZE);
    if (pthread_mutex_lock(&socket_mutex)!=0) throw THREAD_ERROR;
    byte_received=read(clfd, buf, BUF_SIZE);
    if (pthread_mutex_unlock(&socket_mutex)!=0) throw THREAD_ERROR;
    if (byte_received < BUF_SIZE)
    {
      buf[byte_received] = '\0';
      done = true;
    }
    request = request + std::string(buf);
  }
  std::cout<<"request="<<request<<std::endl;

  // pick a leveldb server to forward request
  // now gateserver acts as client to leveldbserver
  char ldbsvrip[INET_ADDRSTRLEN] = "192.168.75.164"; //TODO: hard coded
  const uint16_t ldbsvrport = 8888;
  client clt(ldbsvrip, ldbsvrport);
  std::string ldback = clt.sendstring(request.c_str());

std::cout<<"ldback="<<ldback<<std::endl;

  if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
  *ackmsg = "REPLY FROM LEVELDB SERVER VIA GATEWAY: "+ldback;
  if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;
  if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
  return 0;
}
