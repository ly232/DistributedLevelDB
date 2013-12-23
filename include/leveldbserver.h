//leveldbserver.h
#ifndef _leveldb_h
#define _leveldb_h

#ifndef _common_h
#include "include/common.h"
#endif

#ifndef _server_h
#include "include/server.h"
#endif

//#ifndef _chord_h
#include "include/chord.h"
//#endif

#ifndef STORAGE_LEVELDB_INCLUDE_DB_H_
#include <leveldb/db.h>
#endif

class leveldbserver : public server
{
public:
  leveldbserver(const uint16_t cluster_svr_port,
		const uint16_t self_port, 
		std::string cluster_svr_ip = "",
		const char self_ip[] = NULL,
		std::string dbdir = "/home/ly232/levdb/db0");
  virtual void requestHandler(int clfd);
  virtual ~leveldbserver();
private:
  //generic interface:
  static void process_leveldb_request(std::string& request,
				      std::string& response,
				      leveldbserver* handle);
  static void* main_thread(void*);
  static void* send_thread(void*);
  static void* recv_thread(void*); 

  //leveldb interface:
  leveldb::DB* db;
  leveldb::Options options;
  leveldb::Status status;

  //cluster server interface:
  std::string _cluster_svr_ip;
  uint16_t _cluster_svr_port;
  void join_cluster();
  void leave_cluster();

  //chord interface:
  chordModuleNS::chordModule* chordhdl; //1 chord thread obj
                         //2 muxes: 1 for dbconn, 1 for cv
                         //1 cv
  syncobj chordso;
  static void* chord_init(void*);
};
#endif

