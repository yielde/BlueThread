#pragma once
#include <pthread.h>

#include <atomic>
#include <chrono>
#include <list>
#include <ratio>
#include <string>

#include "synchronization/shared_mutex.h"

/*thread heartbeat data struct*/

struct heartbeat_handle {
  const std::string name;
  pthread_t thread_id = 0;
  using clock = std::chrono::steady_clock;
  using time = std::chrono::time_point<clock>;
  std::atomic<time> timeout = time::min();
  std::atomic<time> suicide_timeout = time::min();
  std::chrono::duration<uint64_t, std::nano> grace =
      std::chrono::duration<uint64_t, std::nano>::zero();
  std::chrono::duration<uint64_t, std::nano> suicide_grace =
      std::chrono::duration<uint64_t, std::nano>::zero();
  std::list<heartbeat_handle*>::iterator list_item;

  explicit heartbeat_handle(const std::string& name) : name(name) {}
};

class HeartbeatMap {
public:
  explicit HeartbeatMap(const std::string& name);
  ~HeartbeatMap();
  heartbeat_handle* add_worker(const std::string& name, pthread_t thread_id);
  void remove_worker(heartbeat_handle* h);
  void reset_timeout(
      heartbeat_handle* h,
      std::chrono::duration<uint64_t, std::nano> grace,
      std::chrono::duration<uint64_t, std::nano> suicide_grace);
  void clear_timeout(heartbeat_handle* h);
  bool is_healthy();
  int get_unhealthy_workers() const;
  int get_total_workers() const;

private:
  using clock = std::chrono::steady_clock;
  using time = std::chrono::time_point<clock>;
  shared_mutex_debug m_rwlock;
  std::list<heartbeat_handle*> m_workers;
  std::atomic<unsigned> m_unhealthy_workers = {0};
  std::atomic<unsigned> m_total_workers = {0};
  bool _check(const heartbeat_handle* h, const char* who, time now) const;
};
