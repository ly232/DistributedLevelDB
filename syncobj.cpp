#include "include/syncobj.h"
syncobj::syncobj(size_t nthread, size_t nmutex, size_t ncv)
{
  int i;
  for (i=0; i<nthread; i++)
  {
    pthread_t thread_obj;
    _thread_obj_arr.push_back(thread_obj);
  }
  for (i=0; i<nmutex; i++)
  {
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    _mutex_arr.push_back(mutex);
  }
  for (i=0; i<ncv; i++)
  {
    pthread_cond_t cv;
    pthread_cond_init(&cv, NULL);
    _cv_arr.push_back(cv);
  }
}
syncobj::~syncobj()
{
}


