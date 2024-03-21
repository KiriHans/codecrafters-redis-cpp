#include <iostream>
#include <cstdlib>

#include <string>
#include <cstring>
#include <cctype>
#include <algorithm>

#include <tuple>

#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <arpa/inet.h>
#include <netdb.h>

#include "RedisParser.cpp"

const int MAX_EVENTS = 10;
const int BUFF_SIZE = 1024;

bool handle_client(int client_fd, int epoll_fd)
{
  char buffer[BUFF_SIZE];
  ssize_t read_client_bytes = read(client_fd, buffer, BUFF_SIZE - 1);

  std::string str(buffer);

  // std::cout << str << " - "  << "count" << std::endl;
  auto [count, message] = RedisParser::parse(str);

  if (read_client_bytes <= 0)
  {
    std::cerr << "An error has ocurred" << std::endl;

    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    return false;
  }

  std::string command_client = message.array[0];

  std::transform(command_client.begin(), command_client.end(), command_client.begin(), ::toupper);
  if (command_client == "PING")
  {
    std::string write_string = "+PONG\r\n";
    int size_string = write_string.size();
    auto sent_fd = write(client_fd, write_string.c_str(), size_string);
  }
  else if (command_client == "ECHO")
  {
    std::string echo_message = message.array[1];
    std::string write_string = "$" + std::to_string(echo_message.size()) + "\r\n" + echo_message + "\r\n";

    int size_string = write_string.size();
    auto sent_fd = write(client_fd, write_string.c_str(), size_string);
  }

  return true;
}

int main(int argc, char **argv)
{
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
  {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0)
  {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";

  int flags = fcntl(server_fd, F_GETFL, 0);
  if (flags == -1)
  {
    std::cerr << "Error getting flags" << std::endl;
    close(server_fd);
    return 1;
  }

  if (fcntl(server_fd, F_SETFL, flags |= O_NONBLOCK) == -1)
  {
    std::cerr << "Error setting sockets to Non-blocking" << std::endl;
    close(server_fd);
    return 1;
  }

  struct epoll_event ev, events[MAX_EVENTS];

  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1)
  {
    std::cerr << "epoll_create1 failed" << std::endl;
    return 1;
  }

  ev.events = EPOLLIN;
  ev.data.fd = server_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1)
  {
    std::cerr << "epoll_create1 failed" << std::endl;
    close(epoll_fd);
    return 1;
  }
  bool running = true;
  while (running)
  {
    int num_fd = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    for (size_t n = 0; n < num_fd; n++)
    {
      if (events[n].data.fd == server_fd)
      {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
        if (client_fd == -1)
        {
          std::cerr << "Accept() failed" << std::endl;
          continue;
        }

        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
        {
          std::cerr << "Failed to add client file descriptor to epoll" << std::endl;
          close(client_fd);
          continue;
        }
      }
      else
      {
        running = handle_client(events[n].data.fd, epoll_fd);
      }
    }
  }

  close(server_fd);
  close(epoll_fd);

  return 0;
}
