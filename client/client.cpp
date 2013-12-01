//client.cpp
#include "include/client.h"
#include "include/syncobj.h"
#include <jsoncpp/json.h>

client::client(const char* remote_ip, 
	       const uint16_t remote_port)
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
    goodconn = (connect(socket_fd, 
			(struct sockaddr*)&svaddr, 
			sizeof(struct sockaddr))==0);
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
std::string client::sendstring(const char* str)
{
  if (!goodconn)
  {
    std::cerr<<"error: bad connection for client. errno="
             <<errno<<std::endl;
    return "connection error";
  }

  syncobj* so = new syncobj(2L, 1L, 0L); 
    //2 threads, 1 mutex, 0 cv

  std::string ackmsg;

  std::vector<void*> thread_arg;
  thread_arg.push_back((void*)str);
  thread_arg.push_back((void*)&socket_fd);
  thread_arg.push_back((void*)so);
  thread_arg.push_back((void*)&ackmsg);

  if (pthread_create(&(so->_thread_obj_arr[0]), 
		     0, 
		     &send_thread, 
		     (void*)&thread_arg)!=0)
    throw THREAD_ERROR;

  if (pthread_join(so->_thread_obj_arr[0], 0)!=0)
    throw THREAD_ERROR;

  if (pthread_create(&(so->_thread_obj_arr[1]), 
		     0, 
		     &recv_thread, 
		     (void*)&thread_arg)
      !=0)
    throw THREAD_ERROR;

  if (pthread_join(so->_thread_obj_arr[1], 0)!=0)
    throw THREAD_ERROR;

  delete so;

  return ackmsg;

}

//!!!NOT SUPPORTED YET
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
  pthread_mutex_t& sock_mutex = ((syncobj*)argvec[2])->_mutex_arr[0];
  try
  {
    if (!str) throw FILE_IO_ERROR;
    size_t rmsz = strlen(str) + 1; //include '\0'
    char* p = const_cast<char*>(str);
    while (rmsz>0)
    {
      size_t cpsz = (rmsz>BUF_SIZE)?BUF_SIZE:rmsz;
      memset(buf,0,BUF_SIZE);
      memcpy(buf, p, cpsz);
      pthread_mutex_lock(&sock_mutex);
      byte_sent = write(sock_fd, buf, cpsz);
      if (byte_sent<0)
      {
        printf("client sendstring write faile. sockfd=%d, errno=%d\n",sock_fd,errno);
        throw FILE_IO_ERROR;
      }
      pthread_mutex_unlock(&sock_mutex);
      p += byte_sent;
      rmsz -= byte_sent;
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
  char buf[BUF_SIZE];
  ssize_t byte_read = -1;
  std::vector<void*> argvec = *(std::vector<void*>*)arg;
  int sock_fd = *(int*)argvec[1];
  pthread_mutex_t& sock_mutex = ((syncobj*)argvec[2])->_mutex_arr[0];

  pthread_mutex_lock(&sock_mutex);
  bool done = false;
  std::string& ackmsg = *(std::string*)argvec[3];

  while (!done)
  {
    memset(buf,0,BUF_SIZE);
    byte_read=read(sock_fd,buf,BUF_SIZE);
    if (buf[byte_read-1]=='\0')
      done = true;
    ackmsg += std::string(buf);
  }
  pthread_mutex_unlock(&sock_mutex);
  if (byte_read==-1)
    std::cerr<<"error waiting for ack"<<std::endl;
  return 0;
}

//async sendstring api
//the caller must provide a syncobj for callee to notify
//when a response is received.
//i.e. caller is blocked until some client responds.
//inputs are created by caller on heap,
//but sendstring_noblock does not free it.
//this is because callee does not know whether req is 
//being shared by other threads calling this sendstring_noblock api
//so the caller must spawn a cleanup thread to free up inputs
void client::sendstring_noblock(const char* req, 
				syncobj* cso, 
				int* numdone,
				int* numtotal,
				std::string* ldback)
{
  if (!goodconn)
  {
    std::cerr
      <<"error: bad connection for client"
      <<std::endl;
    return;
  }
  pthread_t main_thread_obj;
  std::vector<void*>* argv = new std::vector<void*>;
    //argv will be deleted by main thread
  argv->push_back((void*)req);
  argv->push_back((void*)cso);
  argv->push_back((void*)numdone);
  argv->push_back((void*)this);
  argv->push_back((void*)numtotal);
  argv->push_back((void*)ldback);

  if(pthread_create(&main_thread_obj, 
		 0, 
		 &main_thread, 
		 (void*)argv)
     !=0)
    throw THREAD_ERROR;
  return;
}

void* client::main_thread(void* arg)
{
  std::vector<void*>* argv = (std::vector<void*>*) arg;
  char* req = (char*)((*argv)[0]);
  syncobj* cso = (syncobj*)((*argv)[1]);
  int* numdone = (int*)((*argv)[2]);
  client* handle = (client*)((*argv)[3]);
  int* numtotal = (int*)((*argv)[4]);
  std::string* ldback = (std::string*)((*argv)[5]);

  pthread_cond_t& cv = cso->_cv_arr[0];
  pthread_mutex_t& cv_mutex = cso->_mutex_arr[0];
  
  *ldback = handle->sendstring(req); //this blocks
  
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(*ldback,root))
  {
    printf("error: json parser failed at client::main_thread\n");
    throw THREAD_ERROR;
  }
  std::string status = root["status"].asString();
  
  if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
  (*numdone)++;
  bool alldone = (*numdone==*numtotal);
  if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;
  if (status=="OK" || alldone) //alldone will reactivate cleanup thread
  {
    if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
  }
}
