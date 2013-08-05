//cltst.cpp
//unit test for client
#include <iostream>
#include "include/client.h"
#include <jsoncpp/json.h>
#include <jsoncpp/reader.h>
#include <jsoncpp/writer.h>
using namespace std;
int main(int argc, char** argv)
{
  if (argc!=3) return 1;
  try
  {
    cout<<"client test"<<endl;
    Json::Value root;
    root["req_type"] = "put";
    root["req_args"]["key"] = "ly232";
    root["req_args"]["value"] = "Lin Yang";
    Json::StyledWriter writer;
    std::string outputConfig = writer.write(root);
    std::cout<<"outputConfig="<<outputConfig<<std::endl;
    client clt(argv[1], atoi(argv[2]));
    std::string reply = clt.sendstring(outputConfig.c_str());
    std::cout<<"reply="<<reply<<std::endl;
  }
  catch(int e)
  {
    cout<<"error code = "<<e<<endl;
  }
  return 0;
}
