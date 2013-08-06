//schema.h
//this file contains the schema for json request/response types
//TODO: this file is not used for this project yet...
#ifndef _schema_h
#define _schema_h
namespace dldb
{
enum dldb_json_schema
{
  //request/response to leveldb:
  PUT = 1,
  GET,
  DELETE,
  EXIT,
  //request/response to cluster server:
  JOIN_CLUSTER,
  LEAVE_CLUSTER
} json_schema_dldb;
} //end of namespace dldb
#endif

