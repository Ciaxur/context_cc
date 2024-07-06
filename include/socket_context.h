#pragma once

#include <exception>
#include <string>

#include "channel.h"

// Only 2 events can ever occur. Either the cancellation channel triggers
// a read I/O event or the underlying socket does.
#define MAX_NUM_EVENTS 2

// SocketContext cancellation exception.
class SocketContextCancelled: public std::exception {
private:
  std::string message;

public:
  explicit SocketContextCancelled(const std::string &msg): message(msg) {}
  const char* what() const noexcept override {
    return this->message.c_str();
  }
};

// General SocketContext error.
class SocketContextError: public std::exception {
private:
  std::string message;

public:
  explicit SocketContextError(const std::string &msg): message(msg) {}
  const char* what() const noexcept override {
    return this->message.c_str();
  }
};


class SocketContext {
private:
  // SocketContext cancellation channel.
  Channel cancel_chan;

  // State of the context's completion.
  bool is_done;

  // Wrapped socket file descriptor;
  int sock_fd;

  // epoll file descriptor used for I/O event polling.
  int epoll_fd;

public:
  /**
  * Constructs a context wrapped file descriptor.
  *
  * @param fd File descriptor to be wrapped around a Context.
  *
  * @throws ChannelError exception upon channel instantation failure.
  */
  explicit SocketContext(int fd);

  /**
   * Desctructor which handles closing the underlying socket.
   */
  ~SocketContext();

  /**
   * Returns the completion state of the context.
   *
   * @returns Boolean representing the completion of the context.
   */
  bool done() const noexcept;

  /**
   * Cancels the Context.
   *
   * @param reason Reason for context cancellation.
   */
  void cancel() noexcept;

  /**
  * Writes data to the underlying socket.
  *
  * @param data Data to write to the socket.
  * @param size Length of data.
  *
  * @throws SocketContextCancelled upon context cancellation or failure to read from underlying socket.
  *
  * @returns Number of bytes written.
  */
  size_t write(const char *data, size_t size);

  /**
  * Reads data off the underlying socket. Blocks until data is read from the socket or
  * upon context cancellation.
  *
  * @param buffer Pointer to the buffer being filled with data.
  * @param bytes_to_read Number of bytes to read from the channel.
  *
  * @throws SocketContextCancelled upon context cancellation.
  * @returns Number of bytes read.
  */
  size_t read(char *buffer, size_t bytes_to_read);
};

