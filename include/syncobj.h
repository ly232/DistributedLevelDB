//syncobj.h

/*
 * resource management class for synchronization objects
 */

#ifndef _syncobj_h
#define _syncobj_h

#include <vector>
#include <pthread.h>

class syncobj
{
public:
  //see example usage pattern in gateserver::main_thread
  syncobj(size_t nthread, //num threads, used in ptread_create
          size_t nmutex,  //num mux
          size_t ncv);    //num cv
  ~syncobj();
  std::vector<pthread_t> _thread_obj_arr;
  std::vector<pthread_mutex_t> _mutex_arr;
  std::vector<pthread_cond_t> _cv_arr;
private:
  syncobj();
  syncobj(const syncobj&);
  const syncobj& operator=(const syncobj&);
  
};
#endif
