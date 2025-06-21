#include <iostream>
#include "IpcDemoServer.hpp"

int main()
{
  std::cout << "Hello and welcome to ipc demo server" << std::endl;

  IpcDemoServer icpDemoServer;

  while (true) {
    icpDemoServer.runLoop();
  }
  return 0;
}