#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H
#include "nlohmann/json.hpp"

#include <string>
#include <vector>

class IpcDemoServer {
  public:
  IpcDemoServer();
  ~IpcDemoServer();
  int setupServerSocket();
  void runLoop();

  protected:
  private:
  int m_server_fd;         /* Socket */
  bool broadcast(const nlohmann::json& obj);
  std::vector<int> m_clients;

};

#endif