//leveldbserver.h
#include _leveldb_h
#define _leveldb_h
#include "common.h"
#include "server.h"
#include <leveldb/db.h>
class leveldbserver : public server
{
public:
  leveldbserver(const uint16_t port, 
	     const char* ip = NULL):server(port, ip){};
  virtual void requestHandler(int clfd);
  virtual ~leveldbserver();
private:
  
  
}
#endif
