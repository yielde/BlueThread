#include "utils.h"
#include <asm-generic/errno-base.h>
#include <atomic>
#include <pthread.h>
#include <system_error>

class mutex_recursive {
private:
  pthread_mutex_t m;
  std::atomic<int> lock_count = 0;
  void _init() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m, &attr);
    pthread_mutexattr_destroy(&attr);
  }

  void _post_lock() { lock_count.fetch_add(1, std::memory_order_release); }
  void _pre_unlock() {
    ASSERT(lock_count.load(std::memory_order_acquire) > 0);
    lock_count.fetch_sub(1, std::memory_order_release);
  }

public:
  mutex_recursive() { _init(); }
  ~mutex_recursive() { pthread_mutex_destroy(&m); }
  void lock() {
    if (try_lock())
      return;
    int r = pthread_mutex_lock(&m);
    if (unlikely(r == EBUSY || r == EPERM || r == EDEADLK))
      throw std::system_error(r, std::generic_category());
    _post_lock();
  }
  mutex_recursive(const mutex_recursive &) = delete;
  mutex_recursive &operator=(const mutex_recursive &) = delete;

  mutex_recursive(mutex_recursive &&) = delete;
  mutex_recursive &operator=(mutex_recursive &&) = delete;

  void unlock() {
    _pre_unlock();
    int r = pthread_mutex_unlock(&m);
    ASSERT(r == 0);
  }

  bool try_lock() {
    int r = pthread_mutex_trylock(&m);
    bool locked = false;
    if (r == 0) {
      locked = true;
    } else if (r == EBUSY) {
      locked = false;
    } else {
      throw std::system_error(r, std::generic_category(),
                              "pthread_mutex_trylock");
    }
    if (locked)
      _post_lock();
    return locked;
  }
};