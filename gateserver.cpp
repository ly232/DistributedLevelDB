//gateserver.cpp
#include "include/gateserver.h"
void gateserver::requestHandler(int clfd)
{
  pthread_t send_thread_obj, recv_thread_obj;

  std::vector<void*> thread_arg;
  thread_arg.push_back((void*)&clfd);
  thread_arg.push_back((void*)&mutex);

  if (pthread_create(&recv_thread_obj, 
		     0, 
		     &recv_thread, 
		     (void*)&thread_arg)
      !=0)
    throw THREAD_ERROR;

  if (pthread_create(&send_thread_obj, 
		     0, 
		     &send_thread, 
		     (void*)&thread_arg)
      !=0)
    throw THREAD_ERROR;

  if (pthread_join(recv_thread_obj, 0)!=0)
    throw THREAD_ERROR;
  if (pthread_join(send_thread_obj, 0)!=0)
    throw THREAD_ERROR;

  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;

}

void* gateserver::send_thread(void* arg)
{
  std::vector<void*> argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t* mutex_addr = (pthread_mutex_t*)argv[1];
  char resp[] = "ack from gate server";
  size_t resp_len = strlen(resp)+1;
  int tmp;
  pthread_mutex_lock(mutex_addr);
  if ((tmp=write(clfd, resp, resp_len))!=resp_len)
  {
    printf("resp_len=%ld, write_ret=%d\n",resp_len, tmp);
    throw FILE_IO_ERROR;
  }
  pthread_mutex_unlock(mutex_addr);
  return 0;
}

void* gateserver::recv_thread(void* arg)
{
  std::vector<void*> argv = *(std::vector<void*>*)arg;
  int clfd = *(int*)argv[0];
  pthread_mutex_t* mutex_addr = (pthread_mutex_t*)argv[1];
  int byte_received = -1;
  std::string request;
  char buf[BUF_SIZE];
  bool done = false;
  while (!done)
  {
    memset(buf, 0, BUF_SIZE);
    pthread_mutex_lock(mutex_addr);
    byte_received=read(clfd, buf, BUF_SIZE);
    pthread_mutex_unlock(mutex_addr);
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
