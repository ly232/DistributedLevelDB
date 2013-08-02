#include <iostream>
#include <assert.h>
#include "leveldb/db.h"

int main()
{
  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  //options.error_if_exists = true;
  leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
  assert(status.ok());
  leveldb::Slice key1("ly232");
  leveldb::Slice key2("cw597");
  db->Put(leveldb::WriteOptions(), key1, "Lin Yang");
  db->Put(leveldb::WriteOptions(), key2, "Chen Wang");
  std::string name;
  db->Get(leveldb::ReadOptions(), key1, &name);
  std::cout<<"key="<<key1.ToString()<<", value="<<name<<std::endl;

  //leveldb::Status s = db->Get(leveldb::ReadOptions(), key1, &value);
  //if (s.ok()) s = db->Put(leveldb::WriteOptions(), key2, value);
  //if (s.ok()) s = db->Delete(leveldb::WriteOptions(), key1);
  return 0;
}
