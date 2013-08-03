//client.cpp
#include "include/client.h"

client::client(const char* remote_ip, const uint16_t remote_port)
{
  try
  {
    //socket construction:
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0))==-1)
      throw SOCKET_CONSTRUCT_ERROR;
    memset(&svaddr, 0, sizeof(struct sockaddr_in));
    svaddr.sin_family = AF_INET;
    inet_pton(AF_INET, remote_ip, (void*)&svaddr.sin_addr);
    svaddr.sin_port = htons(remote_port);
    connect(socket_fd, (struct sockaddr*)&svaddr, sizeof(struct sockaddr));
  }
  catch(int e)
  {
    throw;
  }
}

client::~client()
{
  if (close(socket_fd)==-1)
    throw SOCKET_CLOSE_ERROR;
}

//multithreaded since it waits for ack
void client::sendstring(const char* str)
{
  pthread_t send_thread_obj, recv_thread_obj;
  //hack: to pack str and socket_fd
  std::vector<void*> thread_arg;
  thread_arg.push_back((void*)str);
  thread_arg.push_back((void*)&socket_fd);
  thread_arg.push_back((void*)&mutex);

  if (pthread_create(&send_thread_obj, 
		     0, 
		     &send_thread, 
		     (void*)&thread_arg)!=0)
    throw THREAD_ERROR;

/*
  //wait for send thread to finish before start recv thread
  if (pthread_join(send_thread_obj, 0)!=0)
    throw THREAD_ERROR;
*/


  if (pthread_create(&recv_thread_obj, 
		     0, 
		     &recv_thread, 
		     (void*)&thread_arg)
      !=0)
    throw THREAD_ERROR;


  //wait for both threads to finish
  if (pthread_join(send_thread_obj, 0)!=0)
    throw THREAD_ERROR;
  if (pthread_join(recv_thread_obj, 0)!=0)
    throw THREAD_ERROR;

}

//single threaded since it doesn't wait for ack, 
//so no need for full duplex
void client::sendfile(const char* fname)
{
  int fd;
  ssize_t byte_sent = -1;
  ssize_t byte_read = -1;
  char buf[BUF_SIZE];
  try
  {
    if ((fd=open(fname, O_RDONLY))==-1)
      throw FILE_IO_ERROR;
    while ((byte_read=read(fd, buf, BUF_SIZE))>0)
    {
      if ((byte_sent=write(socket_fd, buf, byte_read))
	  !=byte_read)
	throw FILE_IO_ERROR;
    }
    if (close(fd)<0)
      throw FILE_IO_ERROR;
    if (byte_sent<0)
      throw SOCKET_SEND_ERROR;
  }
  catch(int e)
  {
    throw;
  }
}

void* client::send_thread(void* arg)
{
  ssize_t byte_sent = -1;
  char buf[BUF_SIZE];
  std::vector<void*> argvec = *(std::vector<void*>*)arg;
  char* str = (char*)argvec[0];
  int sock_fd = *(int*)argvec[1];
  pthread_mutex_t* mutex_addr = (pthread_mutex_t*)argvec[2];
  try
  {
    if (!str) throw FILE_IO_ERROR;
    size_t rmsz = strlen(str);
    char* p = const_cast<char*>(str);
    while (rmsz>0)
    {
      size_t cpsz = (rmsz>BUF_SIZE)?BUF_SIZE:rmsz;
      rmsz -= cpsz;
      memcpy(buf, p, cpsz);
      pthread_mutex_lock(mutex_addr);
      if ((byte_sent = write(sock_fd, buf, cpsz))
	  !=cpsz) throw FILE_IO_ERROR;
      pthread_mutex_unlock(mutex_addr);
      p+=cpsz;
    }
  }
  catch (int e)
  {
    throw;
  }
  return 0;
}

void* client::recv_thread(void* arg)
{
printf("inside client recv thread\n");
  char buf[BUF_SIZE];
  std::vector<void*> argvec = *(std::vector<void*>*)arg;
  int sock_fd = *(int*)argvec[1];
  pthread_mutex_t* mutex_addr = (pthread_mutex_t*)argvec[2];
  pthread_mutex_lock(mutex_addr);
  ssize_t byte_read = read(sock_fd, buf, BUF_SIZE);
  pthread_mutex_unlock(mutex_addr);
  if (byte_read==-1) printf("error for wait ack\n");
  else printf("ack=%s\n",buf);
  return 0;
}


