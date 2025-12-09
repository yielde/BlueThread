#ifndef UTILS_H
#define UTILS_H
#include "backtrace.h"
#include <chrono>
#include <iostream>

#define unlikely(x) __builtin_expect((x), 0)

#define ASSERT(x)                                                              \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << "Assertion failed: " << #x << " at " << __FILE__ << ":"     \
                << __LINE__ << ":" << __func__ << std::endl;                   \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#define ABORT(msg)                                                             \
  do {                                                                         \
    std::cerr << "Abort: " << msg << " at " << __FILE__ << ":" << __LINE__     \
              << ":" << __func__ << std::endl;                                 \
    abort();                                                                   \
  } while (0)

int lockdep_register(const std::string &name);
int lockdep_will_lock(const std::string &name, int id);
int lockdep_locked(const std::string &name, int id);
void lockdep_unregister(int id);
int lockdep_will_unlock(const std::string &name, int id);

/* used by conditon_variable */
template <typename Clock, typename Duration>
timespec
to_timespec(const std::chrono::time_point<Clock, Duration> &time_point) {
  timespec ts;
  auto epoch = time_point.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
  auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(epoch - seconds);
  ts.tv_sec = seconds.count();
  ts.tv_nsec = nanoseconds.count();
  return ts;
}
#endif