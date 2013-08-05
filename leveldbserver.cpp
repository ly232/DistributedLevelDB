//leveldbserver.cpp
#include "include/leveldbserver.h"
#include "include/syncobj.h"
#include <leveldb/db.h>
#include <algorithm>

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
  std::vector<void*>* argv = (std::vector<void*>*)arg;

  int* clfdptr = (int*)((*argv)[0]);
  int clfd = *clfdptr;
  leveldbserver* ldbsvr = (leveldbserver*)((*argv)[1]);

  //TODO: 
  //currently we close tcp conn once the current request is complete
  //we do not maintain tcp conn as in a distributed setting the next fetch
  //may or may not request to on the same node.
  //as an enhancement in the future, we may explore when to keep conn.
    std::cout<<"main thread ready"<<std::endl;

    syncobj* so = new syncobj(2L, 2L, 1L);
    std::string* ackmsg = new std::string;

    std::vector<void*> thread_arg;
    thread_arg.push_back((void*)&clfd);
    thread_arg.push_back((void*)so);
    thread_arg.push_back((void*)ackmsg);
    thread_arg.push_back((void*)ldbsvr);
  
    if (pthread_create(&so->_thread_obj_arr[0], 
		       0, 
		       &recv_thread, 
		       (void*)&thread_arg)
	!=0)
      throw THREAD_ERROR;
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

    delete so;
    delete ackmsg;

//std::cout<<"ldbsvr received client exit"<<std::endl;

  delete clfdptr;
  delete (std::vector<void*>*)arg;
  //delete client_exit;

  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
  return 0;
}

void leveldbserver::requestHandler(int clfd)
{
  pthread_t main_thread_obj;
  int* clfdptr = new int; //clfdptr will be deleted by main thread
  *clfdptr = clfd;
  std::vector<void*>* argv = new std::vector<void*>; 
    //argv will be deleted by main thread
  argv->push_back((void*)clfdptr);
  argv->push_back((void*)this);
  if(pthread_create(&main_thread_obj, 
		 0, 
		 &main_thread, 
		 (void*)argv)
     !=0)
    throw THREAD_ERROR;
}

void* leveldbserver::send_thread(void* arg)
{
printf("in send thread\n");
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

//this thread interacts with leveldb layer
void* leveldbserver::recv_thread(void* arg)
{
printf("in recv thread\n");

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
  leveldbserver* ldbsvr = (leveldbserver*)argv[3];
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
    else if(buf[byte_received-1]=='\0')
      done = true;
    
    request = request + std::string(buf);
  }
  //std::cout<<"request="<<request<<std::endl;
  
  std::string response;
  
  process_leveldb_request(request,response,ldbsvr);

  if (pthread_mutex_lock(&cv_mutex)!=0) throw THREAD_ERROR;
  *ackmsg = response;//ldbsvr->status.ToString();
  if (pthread_mutex_unlock(&cv_mutex)!=0) throw THREAD_ERROR;
  if (pthread_cond_signal(&cv)!=0) throw THREAD_ERROR;
  return 0;
}

void leveldbserver::process_leveldb_request(std::string& request,
					    std::string& response,
					    leveldbserver* ldbsvr)
{
  Json::Value root;
  Json::Reader reader;
  Json::StyledWriter writer;
  if (!reader.parse(request,root))
  {
    ldbsvr->status = 
      leveldb::Status::InvalidArgument("Json parse error");
    root.clear();
    root["result"] = "";
    root["status"] = ldbsvr->status.ToString();
    response = writer.write(root);
    return;
  }
  std::string req_type = root["req_type"].asString();
  std::transform(req_type.begin(), 
		 req_type.end(), 
		 req_type.begin(), 
		 ::tolower);
  if (req_type=="put")
  {
    std::string key = root["key"].asString();
    std::string value = root["value"].asString();
    ldbsvr->status = 
      ldbsvr->db->Put(leveldb::WriteOptions(),key,value);
    root.clear();
    root["result"] = "";
  }
  else if (req_type=="get")
  {
    std::string key = root["key"].asString();
    std::string value;
    ldbsvr->status = 
      ldbsvr->db->Get(leveldb::ReadOptions(),key,&value);
    root.clear();
    root["result"] = value;
  }
  else if (req_type=="delete")
  {
    std::string key = root["key"].asString();
    ldbsvr->status = 
      ldbsvr->db->Delete(leveldb::WriteOptions(),key);
    root.clear();
    root["result"] = "";
  }
  else if (req_type=="exit")
  {
    ldbsvr->status = leveldb::Status::OK();
    root.clear();
    root["result"] = "";
  }
  else
  {
    ldbsvr->status = 
      leveldb::Status::InvalidArgument(request);
    root.clear();
    root["result"] = "";
  } 
  root["status"] = ldbsvr->status.ToString();
  response = writer.write(root);

}
