#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <arpa/inet.h>
#include <netdb.h>

const int MAX_EVENTS = 10;
const int BUFF_SIZE = 1024;

void handle_client(int client_fd)
{
  char buffer[BUFF_SIZE];
  ssize_t read_client_bytes = read(client_fd, buffer, BUFF_SIZE - 1);
  if (read_client_bytes <= 0)
  {
    std::cerr << "An error has ocurred" << std::endl;
  }

  std::string write_string = "+PONG\r\n";
  int size_string = strlen(write_string.c_str());
  auto sent_fd = write(client_fd, write_string.c_str(), size_string);
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

  while (true)
  {
    int num_fd = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    std::cout << "Test" << std::endl;
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
      } else {
        handle_client(events[n].data.fd);
      }
    }
  }

  close(server_fd);
  close(epoll_fd);

  return 0;
}
