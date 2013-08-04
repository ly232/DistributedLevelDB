//syncobj.h

/*
 * resource management class for synchronization objects
 */

#ifndef _syncobj_h
#define _syncobj_h
#include <pthread.h>
#include <vector>
class syncobj
{
public:
  //mutexes and cond vars used by gateserver threads
  syncobj(size_t nthread, size_t nmutex, size_t ncv);
  ~syncobj();
  std::vector<pthread_t> _thread_obj_arr;
  std::vector<pthread_mutex_t> _mutex_arr;
  std::vector<pthread_cond_t> _cv_arr;
  /*
  pthread_t send_thread_obj;
  pthread_t recv_thread_obj;
  pthread_mutex_t* socket_mutex;
  pthread_mutex_t* ldbcv_mutex;
  pthread_cond_t* ldbcv;
  */
private:
  syncobj();
  syncobj(const syncobj&);
  const syncobj& operator=(const syncobj&);
  
};
#endif
