#pragma once

#include "utils.h"
#include <asm-generic/errno.h>
#include <atomic>
#include <cerrno>
#include <pthread.h>
#include <string>
#include <thread>

class mutex_debug_base {
protected:
  std::string lock_name;
  int id = -1;
  bool lockdep;
  std::atomic<int> locked_count = 0;
  std::thread::id locked_by = {};
  bool _enable_lockdep() const { return lockdep; }

  mutex_debug_base(const std::string &name, bool lockdep = false)
      : lock_name(name), lockdep(lockdep) {
    if (lockdep) {
      _register();
    }
  }
  ~mutex_debug_base() {
    ASSERT(locked_count == 0);
    if (lockdep) {
      _unregister();
    }
  }

public:
  bool is_locked() const { return locked_count > 0; }
  bool is_locked_by_me() const {
    return locked_count.load(std::memory_order_acquire) > 0 &&
           locked_by == std::this_thread::get_id();
  }
  operator bool() const { return is_locked_by_me(); }
  void _register();
  void _unregister();
  void _will_lock();
  void _locked();
  void _will_unlock();
  void _unlocked();
};

class mutex_debug_impl : public mutex_debug_base {
private:
  pthread_mutex_t m;
  void _init() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    int r;
    r = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    ASSERT(r == 0);
    r = pthread_mutex_init(&m, &attr);
    ASSERT(r == 0);
  }

public:
  mutex_debug_impl(const std::string &lock_name, bool lockdep)
      : mutex_debug_base(lock_name, lockdep) {
    _init();
  }
  ~mutex_debug_impl() {
    int r = pthread_mutex_destroy(&m);
    ASSERT(r == 0);
  }
  // mutex concept is non-copyable
  mutex_debug_impl(const mutex_debug_impl &) = delete;
  mutex_debug_impl &operator=(const mutex_debug_impl &) = delete;
  // mutex concept is non-movable
  mutex_debug_impl(mutex_debug_impl &&) = delete;
  mutex_debug_impl &operator=(mutex_debug_impl &&) = delete;

  std::string get_name() const { return lock_name; }

  void lock_impl() {
    int r = pthread_mutex_lock(&m);
    if (unlikely(r == EPERM || r == EDEADLK || r == EBUSY)) {
      throw std::system_error(r, std::generic_category(), "pthread_mutex_lock");
    }
    ASSERT(r == 0);
  }

  void unlock_impl() {
    int r = pthread_mutex_unlock(&m);
    ASSERT(r == 0);
  }

  bool try_lock_impl() {
    int r = pthread_mutex_trylock(&m);
    switch (r) {
    case 0:
      return true;
    case EBUSY:
      return false;
    default:
      throw std::system_error(r, std::generic_category(),
                              "pthread_mutex_trylock");
    }
  }

  void _post_lock() {
    ASSERT(locked_count == 0);
    locked_by = std::this_thread::get_id();
    locked_count.fetch_add(1, std::memory_order_release);
  }

  void _pre_unlock() {
    ASSERT(locked_count == 1);
    if (locked_count == 1)
      locked_by = std::this_thread::get_id();
    locked_count.fetch_sub(1, std::memory_order_release);
  }

  bool try_lock() {
    bool locked = try_lock_impl();
    if (locked) {
      if (lockdep) {
        _locked();
      }
      _post_lock();
    }
    return locked;
  }

  void lock() {
    if (lockdep)
      _will_lock();
    if (try_lock())
      return;
    lock_impl();
    if (lockdep)
      _locked();
    _post_lock();
  }

  void unlock() {
    _pre_unlock();
    if (lockdep)
      _will_unlock();
    unlock_impl();
  }

  pthread_mutex_t *native_handle() { return &m; }
};

using BlueMutex = mutex_debug_impl;