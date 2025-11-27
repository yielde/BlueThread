#pragma once
#include <chrono>
#include <atomic>
#include <pthread.h>
#include <ratio>
#include <string>
#include <list>
/*thread heartbeat data struct*/

struct heartbeat_handle{
  const std::string name;
  pthread_t thread_id = 0;
  using clock = std::chrono::steady_clock;
  using time = std::chrono::time_point<clock>;
  std::atomic<time> timeout = time::min();
  std::atomic<time> suicide_timeout = time::min();
  std::chrono::duration<uint64_t, std::nano> grace = std::chrono::duration<uint64_t, std::nano>::zero();
  std::chrono::duration<uint64_t, std::nano> suicide_grace = std::chrono::duration<uint64_t, std::nano>::zero();
  std::list<heartbeat_handle*>::iterator list_item;
  explicit heartbeat_handle(const std::string &name) : name(name) {}

};

class HeartbeatMap {
  private:
  using clock = std::chrono::steady_clock;
  
};