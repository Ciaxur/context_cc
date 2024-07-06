#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "include/socket_context.h"

void start_consumer(SocketContext *ctx) {
  try {
    while(!ctx->done()) {
      char buffer[255];
      size_t data_read = ctx->read(buffer, 255);

      spdlog::info("Read {}B from client -> '{}'", data_read, buffer);
    }
  } catch(SocketContextCancelled err) {
    spdlog::info("Consumer: Context cancelled -> {}", err.what());
  } catch(SocketContextError err) {
    spdlog::info("Consumer: Context error -> {}", err.what());
  }
  spdlog::info("Closing consumer...");
}

int create_client() {
  int sock;
  struct sockaddr_in srv_addr;

  if ( (sock = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
    perror("Failed to create client socket");
    exit(EXIT_FAILURE);
  }

  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port   = htons(6969);

  if (inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr) < 0) {
    perror("inet_pton");
    exit(EXIT_FAILURE);
  }

  if ( connect(sock, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) == -1 ) {
    perror("Failed to connect to server");
    exit(EXIT_FAILURE);
  }

  return sock;
}

void start_producer(SocketContext *ctx) {
  try {
    while(!ctx->done()) {
      const char* msg = "hello from client";
      ctx->write(msg, strlen(msg));
      spdlog::info("Producer: Wrote '{}' to server", msg);

      char buffer[255];
      spdlog::info("Producer: Waiting on server...");
      size_t bytes_read = ctx->read(buffer, 255);
      spdlog::info("Producer: Read {}B from server", bytes_read);
    }
  } catch(SocketContextCancelled err) {
    spdlog::info("Producer: Context cancelled -> {}", err.what());
  } catch(SocketContextError err) {
    spdlog::info("Producer: Context error -> {}", err.what());
  }
  spdlog::info("Closing producer...");
}

std::tuple<int, sockaddr_in> create_server() {
  int sock_fd;

  if ( (sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0 ) {
    perror("Failed to create server socket");
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }


  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
  address.sin_port = htons(6969);

  if (bind(sock_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("Failed to bind");
    exit(EXIT_FAILURE);
  }

  return std::make_tuple(sock_fd, address);
}

int main() {
  std::tuple<int, sockaddr_in> svr = create_server();
  int server_sock = std::get<0>(svr);
  sockaddr_in svr_addr = std::get<1>(svr);
  socklen_t addrlen = sizeof(svr_addr);

  // Listen for one client.
  if (listen(server_sock, 1) == -1) {
    perror("Failed to listen");
    exit(EXIT_FAILURE);
  }

  // Create a client socket that connects to this server and run in producer.
  spdlog::info("Starting producer...");
  SocketContext* client_ctx = new SocketContext(create_client());
  std::thread producer_t{start_producer, client_ctx};

  // Start accepting client connections.
  int client_sock;
  if( ( client_sock = accept(server_sock, (struct sockaddr*)&svr_addr, &addrlen) ) == -1 ) {
    perror("Failed to accept");
    exit(EXIT_FAILURE);
  }

  spdlog::info("Starting consumer...");
  SocketContext* server_ctx = new SocketContext(client_sock);

  std::thread consumer_t{start_consumer, server_ctx};
  std::this_thread::sleep_for(std::chrono::seconds(3));

  spdlog::info("Broadcasting closure");
  server_ctx->cancel();
  consumer_t.join();
  delete server_ctx;

  producer_t.join();
  delete client_ctx;

  spdlog::info("Cleaning up...");
  if (close(server_sock) == -1)
    perror("Failed to close server socket");


  return 0;
}
