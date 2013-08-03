//gateserver.cpp
#include "include/gateserver.h"
void gateserver::requestHandler(int clfd)
{
  int byte_received = -1;
  memset(buf, 0, BUF_SIZE);
  std::string request;
  memset(buf, 0, BUF_SIZE);
  while ((byte_received=read(clfd, buf, BUF_SIZE))>0)
  {
    if (byte_received < BUF_SIZE)
      buf[byte_received] = '\0';
    request = request + std::string(buf);
  }
  std::cout<<"request="<<request<<std::endl;

  std::string ack = "ack";
  if (write(socket_fd,ack.c_str(),4)!=4)
    std::cerr<<"ack failed"<<std::endl;

  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
};

