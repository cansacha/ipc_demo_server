#include <iostream>
#include "IpcDemoServer.hpp"

// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
int main()
{
  std::cout << "Hello and welcome to DEvice manager" << std::endl;

  IpcDemoServer deviceManager;

  while (true) {
    deviceManager.runLoop();
  }
  return 0;
}