//fileserver.cpp
#include "include/fileserver.h"
void fileserver::requestHandler(int clfd)
{
  std::cout<<"file server request handler"<<std::endl;
  int byte_received = 0;
  int fd = open("./recv.out", O_WRONLY | O_CREAT | O_TRUNC );
  if (fd<0)
    throw FILE_IO_ERROR;
  memset(buf, 0, BUF_SIZE);
  while ((byte_received=read(clfd, buf, BUF_SIZE))>0)
  {
    if (write(fd, buf, byte_received)!=byte_received)
	throw FILE_IO_ERROR;
  }
  if (close(fd)<0)
    throw FILE_IO_ERROR;
  if (close(clfd)<0)
    throw SOCKET_CLOSE_ERROR;
  
};
