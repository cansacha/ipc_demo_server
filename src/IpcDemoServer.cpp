#include "IpcDemoServer.hpp"
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>
#include <sys/un.h>
#include <cstring>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include "Version.hpp"

using json = nlohmann::json;

#define SOCK_PATH "/tmp/ipc_demo_server.sock"

static void set_non_blocking(int fd);
void handle_cmd(std::string command, int fd);
bool send_to_client(int fd, const json& obj);
IpcDemoServer::IpcDemoServer() {

  /* Create the socket */
  m_server_fd = setupServerSocket();
  std::cout << "Listening on " << SOCK_PATH << std::endl;

}

IpcDemoServer::~IpcDemoServer() {

}

/**
 * @brief Creates and sets up the server's UNIX domain socket.
 *
 * This function performs the following steps:
 *   1. Creates a UNIX domain socket (`AF_UNIX`, `SOCK_STREAM`)
 *   2. Unlinks any existing socket file at the same path (`SOCK_PATH`)
 *   3. Binds the socket to the specified path
 *   4. Marks the socket as passive (listen mode) to accept incoming connections
 *   5. Sets the socket to non-blocking mode
 *
 * If any system call fails (socket, bind, listen), the function prints an error
 * and terminates the program via `std::exit(EXIT_FAILURE)`.
 *
 * @return The file descriptor of the configured, non-blocking server socket.
 */
int IpcDemoServer::setupServerSocket() {

  /* Create the UNIX socket */
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) { perror("socket"); std::exit(EXIT_FAILURE); }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path)-1);
  unlink(SOCK_PATH);

  /* Bind phase */
  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    perror("bind"); std::exit(EXIT_FAILURE);
  }

  /*Make socket pasive socket */
  if (listen(fd, 5) < 0) {
    perror("listen"); std::exit(EXIT_FAILURE);
  }
  set_non_blocking(fd);          // ← le socket serveur n’attendra plus indéfiniment
  return fd;

}

/**
 * @brief Main event loop that handles client connections and communication.
 *
 * This function uses `select()` to monitor:
 *   - the listening socket (`m_server_fd`) for new incoming connections
 *   - all active client sockets for incoming data or disconnections
 *
 * When `m_server_fd` is marked as readable by `select()`, it means a new client
 * is waiting in the kernel backlog, and `accept()` is called to establish the connection.
 *
 * When a client socket is marked as readable, `read()` is used to receive data.
 * If the read returns 0 or an error, the client is considered disconnected and is removed.
 * Otherwise, function call handle_cmd() function.
 *
 * All sockets are set to non-blocking mode to avoid stalling the loop.
 * The loop blocks efficiently with `select()`, consuming 0% CPU when idle.
 *
 * This function runs indefinitely until `select()` returns an unrecoverable error.
 */
void IpcDemoServer::runLoop() {
  while (true) {

    /* Build the initial read set:
     *   - FD_ZERO()      : clear the set
     *   - FD_SET()       : always watch the listening socket (m_server_fd)
     *   - max_fd         : start the “highest-fd” tracker with m_server_fd,
     *                      it will be updated while adding client fds.
     */
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(m_server_fd, &read_fds);
    int max_fd = m_server_fd;

    /* Add each persistent client to the set */
    for (int fd : m_clients) {
      FD_SET(fd, &read_fds);
      if (fd > max_fd) max_fd = fd;
    }

    /* Wait for activity on either the listening socket or any connected client.
     * select() blocks until at least one socket becomes readable:
     *   - m_server_fd: means a new client is waiting → call accept()
     *   - client_fd  : means a client sent data, closed the connection, or triggered an error
     * We pass (max_fd + 1) because select() checks fds in range [0, max_fd].
     * Passing NULL as timeout blocks indefinitely (no CPU usage when idle).
     */
    if (select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr) < 0) {
      if (errno == EINTR)   continue;   // interrompu par un signal ? on repart
      perror("select");
      break;                            // autre erreur : on quitte proprement
    }

    /* FD_ISSET tells us that the client socket is readable:
      * this may mean data is available, or the client has disconnected or errored.
      */
    if (FD_ISSET(m_server_fd, &read_fds)) {
      int client_fd = accept(m_server_fd, nullptr, nullptr);
      if (client_fd >= 0) {
        set_non_blocking(client_fd);
        m_clients.push_back(client_fd);
        std::cout << "Client add " << client_fd << std::endl;
      }
    }

    /* Check all existing clients for incoming data or disconnects */
    for (auto it = m_clients.begin(); it != m_clients.end();) {
      int fd = *it;

      /* FD_ISSET tells us that the socket is readable:
       * this may mean data is available, or the client has disconnected or errored.
       */
      if (FD_ISSET(fd, &read_fds)) {
        /* Activity detected */
        char buff[1024];
        ssize_t n = read(fd, buff, sizeof(buff));

        if (n > 0) {
          std::string cmd(buff, n);
          handle_cmd(cmd, fd);
          it++; /* move to the next client */
        }
        else {
          /* Activity has been detected but the client has nothing to say, so it is closed or in error */
          std::cout << "Client -1 (" << fd << ")" << std::endl;
          close(fd);
          it = m_clients.erase(it); /* remove and continue with next */
          continue;
        }
      }
      else {
        it++; /* no activity for this client */
      }

    }

  }
}

void handle_cmd(std::string command, int fd) {
  json j;
  try {j = json::parse(command);}
  catch(...) {
    json error = {
      {"status", "error"},
      {"error", {
          {"1", "malformed"}
      }}
    };
    send_to_client(fd, error);

    return;
  }

  const std::string cmd = j.value("cmd", "");

  if (cmd == "VERSION") {
    json version   = { {"version",IPC_DEMO_SERVER_VERSION} };
    send_to_client(fd, version);
  }



}

bool send_to_client(int fd, const json& obj)
{
  std::string payload = obj.dump();   // sérialise en texte
  payload.push_back('\n');            // framing « ligne »

  ssize_t n = write(fd, payload.data(), payload.size());
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // Socket saturé : à améliorer avec un buffer d’envoi si nécessaire
      return false;
    }
    perror("write");
    close(fd);
    return false;
  }
  return true;
}

/**
 * @brief Sets a file descriptor to non-blocking mode.
 *
 * This function modifies the flags of the given file descriptor so that I/O
 * operations like read() and write() will not block. Instead, they return
 * immediately with -1 and errno set to EAGAIN or EWOULDBLOCK when no data
 * is available.
 *
 * This is required for asynchronous I/O handling with select(), poll(), or epoll().
 *
 * @param fd The file descriptor to set as non-blocking.
 */
static void set_non_blocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) { perror("fcntl F_GETFL"); return; }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    perror("fcntl F_SETFL");
}

bool IpcDemoServer::broadcast(const json& obj) {
  bool ret_value = true;
  for (int fd : m_clients) {
    if (send_to_client(fd, obj) == false) {
      ret_value = false;
    }
  }
  return ret_value;
}

