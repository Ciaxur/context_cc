#include "socket_context.h"

#include <cstdio>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>

SocketContext::SocketContext(int fd): cancel_chan(), is_done(false), sock_fd(fd) {
  // Create an epoll file descriptor to poll I/O events.
  this->epoll_fd = epoll_create1(0);
  if (this->epoll_fd == -1) {
    perror("Failed to create epoll fd");
    throw SocketContextError("Failed to create an epoll file descriptor");
  }

  // Track the cancellation channel's read pipe to the epoll.
  struct epoll_event epoll_cancel_event;
  epoll_cancel_event.events  = EPOLLIN,
  epoll_cancel_event.data.fd = this->cancel_chan.pipe_fds[0];

  if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->cancel_chan.pipe_fds[0], &epoll_cancel_event) == -1) {
    perror("Failed to add cancellation channel to epoll");
    throw SocketContextError("Failed to add cancellation channel to epoll");
  }

  // Track the given socket file descriptor to the epoll.
  struct epoll_event epoll_sock_fd_event;
  epoll_sock_fd_event.events = EPOLLIN;
  epoll_sock_fd_event.data.fd = this->sock_fd;

  if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->sock_fd, &epoll_sock_fd_event) == -1) {
    perror("Failed to add underlying socket to epoll");
    throw SocketContextError("Failed to add underlying socket to epoll");
  }
}

SocketContext::~SocketContext() {
  if(close(this->sock_fd) == -1)
    perror("Failed to close underlying socket");

  if (close(this->epoll_fd) == -1)
    perror("Failed to close epoll file descriptor");
}

bool SocketContext::done() const noexcept { return this->is_done; }

void SocketContext::cancel() noexcept {
  // Write event to the cancellation channel, which will trigger an
  // I/O event.
  const char* cancel_event = "cancel";
  this->cancel_chan.write(cancel_event, strlen(cancel_event));
  this->is_done = true;
}

size_t SocketContext::write(const char *data, size_t size) {
  if (this->done()) throw SocketContextCancelled("context closed");

  ssize_t bytes_written = ::write(this->sock_fd, data, size);
  if (bytes_written == -1) {
    perror("Failed to write to socket");
    this->cancel();
  }

  return bytes_written;
}

size_t SocketContext::read(char *buffer, size_t bytes_to_read) {
  if (this->done()) throw SocketContextCancelled("context already closed");

  // Poll for socket I/O events. Either the cancellation channel or socket will trigger an event.
  struct epoll_event events[MAX_NUM_EVENTS];

  int num_events = epoll_wait(this->epoll_fd, events, MAX_NUM_EVENTS, -1);
  if (num_events == -1) {
    perror("epoll_wait failed");
    this->cancel();
    throw SocketContextError("Failed to poll for I/O events");
  }

  for (int i = 0; i < num_events; i++) {
    if (events[i].data.fd == this->cancel_chan.pipe_fds[0]) {
      this->cancel();
      throw SocketContextCancelled("context cancelled");
    }
  }

  ssize_t bytes_read = ::read(this->sock_fd, buffer, bytes_to_read);

  if (bytes_read == -1) {
    perror("Failed to read from underlying socket");
    this->cancel();
    throw SocketContextError("Failed to read from underlying socket");
  }

  else if (bytes_read == 0) {
    this->cancel();
    throw SocketContextCancelled("context cancelled");
  }

  // Add a null terminator.
  buffer[bytes_read] = '\0';
  return bytes_read;
}


