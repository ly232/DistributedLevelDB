//gateserver.cpp
#include "include/gateserver.h"
void gateserver::requestHandler(int clfd)
{
  int byte_received = 0;
  memset(buf, 0, BUF_SIZE);
  std::string request;
  while ((byte_received=read(clfd, buf, BUF_SIZE))>0)
  {
    if (byte_received < BUF_SIZE)
      buf[byte_received] = '\0';
    request = request + std::string(tmp);
  }
  std::cout<<"request="<<request<<std::endl;
  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
};

