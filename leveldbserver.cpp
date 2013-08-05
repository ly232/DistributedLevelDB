//leveldbserver.cpp
#include "include/leveldbserver.h"
#include "include/syncobj.h"
#include <leveldb/db.h>

leveldbserver::leveldbserver(const uint16_t port, 
			     const char ip[],
			     std::string dbdir)
 :server(port, ip)
{
  options.create_if_missing = true;
  status = leveldb::DB::Open(options, dbdir, &db);
  if (!status.ok())
  {
    std::cerr<<"error: leveldb open fail"<<std::endl;
    throw DB_FAIL;
  }
}

leveldbserver::~leveldbserver()
{
  delete db;
}

void* leveldbserver::main_thread(void* arg)
{
  int clfd = *(int*)arg;
  syncobj* so = new syncobj(2L, 2L, 1L);
  std::string* ackmsg = new std::string;
  std::vector<void*> thread_arg;
  thread_arg.push_back((void*)&clfd);
  thread_arg.push_back((void*)so);
  thread_arg.push_back((void*)ackmsg);
  
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

  delete (int*) arg;
  delete so;
  delete ackmsg;

std::cout<<"ldbsvr main thread done"<<std::endl;

  //fsync(clfd);
  
  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
  return 0;
}

void leveldbserver::requestHandler(int clfd)
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

void* leveldbserver::send_thread(void* arg)
{
  std::vector<void*>& argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t& socket_mutex
    = ((syncobj*)argv[1])->_mutex_arr[0];
  pthread_mutex_t& cv_mutex
    = ((syncobj*)argv[1])->_mutex_arr[1];
  pthread_cond_t& cv = ((syncobj*)argv[1])->_cv_arr[0];
  std::string& resp_str = (*(std::string*)argv[2]);
std::cout<<"2"<<std::endl;
  //wait until recv thread finishes reception 
  //and get a response from leveldb server
  if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
  while(resp_str.empty())
  {
    pthread_cond_wait(&cv, &cv_mutex);
  }
std::cout<<"leveldb server reply thread woke up."<<std::endl;
  const char* resp = resp_str.c_str();
  size_t resp_len = strlen(resp)+1;
  if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;

  if (pthread_mutex_lock(&socket_mutex)!=0) throw THREAD_ERROR;
  if (write(clfd,resp,resp_len)!=resp_len) throw FILE_IO_ERROR;
  if (pthread_mutex_unlock(&socket_mutex)!=0) throw THREAD_ERROR;

  return 0;
}

//this thread interacts with leveldb layer
void* leveldbserver::recv_thread(void* arg)
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

  if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
  *ackmsg = "leveldb server not ready yet...";
  if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;
  if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
  std::cout<<"after recv signal"<<std::endl;
  return 0;
}
