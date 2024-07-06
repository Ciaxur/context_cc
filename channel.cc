#include "include/channel.h"

Channel::Channel() {
  if (pipe(this->pipe_fds) == -1) {
    perror("Failed to create pipe");
    throw ChannelError("Failed to create cannel pipe");
  }
}

Channel::~Channel() {
  std::lock_guard<std::mutex> lock(this->write_mtx);
  if(close(pipe_fds[0]))
    perror("Failed to close read pipe");
  if(close(pipe_fds[1]))
    perror("Failed to close write pipe");
}

void Channel::write(const char *data, size_t size) {
  std::lock_guard<std::mutex> lock(this->write_mtx);
  ::write(this->pipe_fds[1], data, size);
}

size_t Channel::read(char *buffer, size_t bytes_to_read) {
  size_t bytes_read = ::read(this->pipe_fds[0], buffer, bytes_to_read);
  if (bytes_read == -1) {
    perror("Failed to read data off the read pipe");
    return 0;
  }
  return bytes_read;
}
