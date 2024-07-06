#pragma once
#include <cstdio>
#include <mutex>
#include <sys/epoll.h>
#include <unistd.h>
#include <exception>
#include <string>


// Forward decleration.
class SocketContext;

// General Channel error.
class ChannelError: public std::exception {
private:
  std::string message;

public:
  explicit ChannelError(const std::string &msg): message(msg) {}
  const char* what() const noexcept override {
    return this->message.c_str();
  }
};

class Channel {
protected:
  // 2 file descriptor pipes:
  // [0] -> Read
  // [1] -> Write
  int pipe_fds[2];

  friend SocketContext;

private:
  std::mutex write_mtx;

public:
  /**
   * Instantiates a channel.
   *
   * @throws ChannelError upon pipe creation failure.
   */
  Channel();
  ~Channel();

  /**
  * Creates a new channel instance. The caller is responsible for cleaning up from heap.
  *
  * @returns Pointer to the created channel or a nullptr upon failure.
  */
  static Channel* create_channel();

  /**
  * Writes data on the pipe. Blocks until data is written to the channel.
  *
  * @param data Data to write into the channel.
  * @param size Length of data.
  */
  void write(const char *data, size_t size);

  /**
  * Reads data off the pipe. Blocks until data is read from the channel.
  *
  * @param buffer Pointer to the buffer being filled with data.
  * @param bytes_to_read Number of bytes to read from the channel.
  *
  * @returns Number of bytes read.
  */
  size_t read(char *buffer, size_t bytes_to_read);
};

