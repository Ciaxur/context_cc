#include <chrono>
#include <cstdio>
#include <mutex>
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <thread>
#include <unistd.h>

struct channel {
  // Read/Write pipes.
  int pipe_fds[2];
  std::mutex mtx;

  void write_data(const char *data, size_t size) {
    std::lock_guard<std::mutex> lock(mtx);
    write(pipe_fds[1], data, size);
  }

  void read_data() {
    //TODO:
  }

  void cleanup() {
    std::lock_guard<std::mutex> lock(mtx);
    if(close(pipe_fds[0]))
      perror("Failed to close read pipe");
    if(close(pipe_fds[1]))
      perror("Failed to close write pipe");
  }
};

struct shared_data {
  int epoll_fd;
  std::mutex mtx;
  channel* done_channel;
  bool is_done = false;

  void cancel() {
    std::lock_guard<std::mutex> lock(mtx);
    const char* done_event = "done";
    done_channel->write_data(done_event, strlen(done_event));
  }

  int close_data() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!is_done) {
      if(close(epoll_fd)) {
        perror("Failed to close epoll fd");
        return -1;
      }
    }
    return 0;
  }
};


channel* create_channel() {
  channel *chan = new channel{};
  if (pipe(chan->pipe_fds) == -1) {
    perror("Failed to create pipe");
    return nullptr;
  }
  return chan;
}

void clean_up(shared_data *sd) {
  if (sd->done_channel) {
    sd->done_channel->cleanup();
    delete sd->done_channel;
  }
  sd->close_data();
}

void start_worker(shared_data *sd) {
  constexpr int MAX_EVENTS = 10;
  struct epoll_event events[MAX_EVENTS];

  while (true) {
    int num_events = epoll_wait(sd->epoll_fd, events, MAX_EVENTS, -1);
    if (num_events == -1) {
      perror("epoll_wait failed");
      return;
    }

    spdlog::info("Event polled!");
    break;
  }
}


// TODO: make sure segfaults are good. cause we not cleaning up nor checking.
int main() {
  shared_data sd{};

  // Create epoll event.
  sd.epoll_fd = epoll_create1(0);
  if (sd.epoll_fd == -1) {
    perror("Failed to create epoll");
    return -1;
  }

  sd.done_channel = create_channel();
  if (sd.done_channel == nullptr) {
    return -1;
  }

  // Add channel to epoll.
  struct epoll_event epoll_event;
  epoll_event.events    = EPOLLIN;
  epoll_event.data.fd   = sd.done_channel->pipe_fds[0];

  if (epoll_ctl(sd.epoll_fd, EPOLL_CTL_ADD, sd.done_channel->pipe_fds[0], &epoll_event) == -1) {
    perror("Failed to add epoll event");
    return -1;
  }

  std::thread t{start_worker, &sd};
  std::this_thread::sleep_for(std::chrono::seconds(3));

  spdlog::info("Broadcasting closure");
  sd.cancel();
  t.join();

  spdlog::info("Cleaning up...");
  clean_up(&sd);
  return 0;
}
