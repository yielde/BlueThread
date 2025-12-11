#include "heartbeat.h"

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>

#include "synchronization/utils.h"

#include "utils.h"

bool
HeartbeatMap::_check(const heartbeat_handle* h, const char* who, time now) const
{
  bool healthy = true;
  if (auto was = h->timeout.load(std::memory_order_relaxed);
      was != time::min() && was < now) {
    std::cout << "Heartbeat hit timeout, for worker: " << who
              << ", had timed out after "
              << std::chrono::duration_cast<std::chrono::milliseconds>(h->grace)
                     .count()
              << std::endl;
    healthy = false;
  }

  if (auto was = h->suicide_timeout.load(std::memory_order_relaxed);
      was != time::min() && was < now) {
    pthread_kill(h->thread_id, SIGABRT);
    sleep(1);
    ABORT("Heartbeat hit suicide timeout, for worker: " << who);
  }
  return healthy;
}

HeartbeatMap::HeartbeatMap(const std::string& name) : m_rwlock(name) {}

HeartbeatMap::~HeartbeatMap() { ASSERT(m_workers.empty()); }

heartbeat_handle*
HeartbeatMap::add_worker(const std::string& name, pthread_t thread_id)
{
  std::unique_lock locker{m_rwlock};
  heartbeat_handle* h = new heartbeat_handle(name);
  m_workers.push_front(h);
  h->list_item = m_workers.begin();
  h->thread_id = thread_id;
  return h;
}

void
HeartbeatMap::remove_worker(heartbeat_handle* h)
{
  std::unique_lock locker{m_rwlock};
  m_workers.erase(h->list_item);
  delete h;
}

void
HeartbeatMap::reset_timeout(
    heartbeat_handle* h,
    std::chrono::duration<uint64_t, std::nano> grace,
    std::chrono::duration<uint64_t, std::nano> suicide_grace)
{
  const auto now = clock::now();
  _check(h, "reset_timeout", now);

  h->timeout.store(now + grace, std::memory_order_relaxed);
  h->grace = grace;

  if (suicide_grace > std::chrono::duration<uint64_t, std::nano>::zero()) {
    h->suicide_timeout.store(now + suicide_grace, std::memory_order_relaxed);
  } else {
    h->suicide_timeout.store(
        time::min(),
        std::memory_order_relaxed); // disable suicide timeout detection
  }
  h->suicide_grace = suicide_grace;
}

void
HeartbeatMap::clear_timeout(heartbeat_handle* h)
{
  auto now = clock::now();
  _check(h, "clear_timeout", now);
  h->timeout.store(time::min(), std::memory_order_relaxed);
  h->suicide_timeout.store(time::min(), std::memory_order_relaxed);
}

bool
HeartbeatMap::is_healthy()
{
  int unhealthy = 0;
  int total = 0;

  m_rwlock.lock_shared();
  auto now = clock::now();

  bool healthy = true;
  for (auto p = m_workers.begin(); p != m_workers.end(); p++) {
    if (!_check(*p, "is_healthy", now)) {
      healthy = false;
      unhealthy++;
    }
    total++;
  }
  m_rwlock.unlock_shared();
  m_unhealthy_workers.store(unhealthy, std::memory_order_relaxed);
  m_total_workers.store(total, std::memory_order_relaxed);
  std::cout << "HeartbeatMap is_healthy: " << healthy
            << ", unhealthy: " << unhealthy << ", total: " << total
            << std::endl;
  return healthy;
}

int
HeartbeatMap::get_unhealthy_workers() const
{
  return m_unhealthy_workers;
}

int
HeartbeatMap::get_total_workers() const
{
  return m_total_workers;
}
