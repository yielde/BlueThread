#pragma once

#include "mutex.h"
#include <atomic>
#include <pthread.h>
#include <string>

class shared_mutex_debug : public mutex_debug_base {
private:
  pthread_rwlock_t rwlock;
  std::atomic<unsigned> read_locked_count = 0;

private:
  void _pre_unlock();
  void _post_lock();
  void _pre_unlock_shared();
  void _post_lock_shared();

public:
  shared_mutex_debug(std::string name, bool lockdep = false,
                     bool prioritize_write = false);
  ~shared_mutex_debug();

  void lock();
  void unlock();
  bool is_wlocked();

  void lock_shared();
  void unlock_shared();
  bool is_rlocked();
};
