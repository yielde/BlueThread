#include "thread.h"
#include "utils.h"
#include <bits/types/sigset_t.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <syscall.h>
#include <system_error>
#include <unistd.h>

pid_t get_tid() { return syscall(SYS_gettid); }

static int _set_cpu_affinity(int cpuid) {
  if (cpuid < 0 || cpuid >= CPU_SETSIZE) {
    std::cout << "invalid cpu id: " << cpuid << std::endl;
    return 0;
  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpuid, &cpuset);
  int r = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
  if (r < 0) {
    std::cout << "failed to set cpu affinity: " << strerror(errno) << std::endl;
    return -errno;
  }
  sched_yield(); // Ensure the CPU affinity takes effect immediately, after
                 // yield the CPU, the scheduler will reschedule the thread to
                 // the new CPU

  return 0;
}

const pthread_t &ThreadBase::get_thread_id() const { return thread_id; }

pid_t ThreadBase::get_tid() const { return tid; }

void *ThreadBase::_entry_func(void *arg) {
  void *r = ((ThreadBase *)arg)->entry_wrapper();
  return r;
}

void *ThreadBase::entry_wrapper() {
  int r = get_tid();
  if (r > 0) {
    tid = r;
  }
  if (tid && cpuid >= 0) {
    int r = _set_cpu_affinity(cpuid);
    if (r == 0) {
      std::cout << "set cpu affinity to " << cpuid << " success"
                << "(tid: " << tid << ")" << std::endl;
    }
  }
  pthread_setname_np(pthread_self(), thread_name.c_str());
  return entry();
}

int ThreadBase::set_cpu_affinity(int cpuid) {
  if (cpuid < 0 || cpuid >= CPU_SETSIZE) {
    std::cout << "invalid cpu id: " << cpuid << std::endl;
    return 0;
  }
  if (tid && get_tid() == tid) {
    int r = _set_cpu_affinity(cpuid);
    return r;
  }
  return 0;
}

int ThreadBase::try_create(int stack_size) {
  pthread_attr_t *thread_attr = NULL;
  pthread_attr_t local_thread_attr;
  if (stack_size) {
    thread_attr = &local_thread_attr;
    pthread_attr_init(thread_attr);
    pthread_attr_setstacksize(thread_attr, stack_size);
  }

  sigset_t old_sigmask;
  block_all_signals(&old_sigmask);
  int r = pthread_create(&thread_id, thread_attr, _entry_func, (void *)this);
  restore_signals(&old_sigmask);
  if (thread_attr) {
    pthread_attr_destroy(thread_attr);
  }
  return r;
}

void ThreadBase::create(const char *thread_name, int stack_size) {
  ASSERT(strlen(thread_name) < 16);
  this->thread_name = thread_name;
  int r = try_create(stack_size);
  if (r != 0) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "failed to create thread(ThreadBase::try_create): %s",
             strerror(r));
    stderr_emergency(buf);
    ASSERT(r == 0);
  }
}

int ThreadBase::join(void **retval) {
  ASSERT(thread_id != 0);
  int r = pthread_join(thread_id, retval);
  if (r != 0) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "failed to join thread(ThreadBase::join): %s",
             strerror(r));
    stderr_emergency(buf);
    ASSERT(r == 0);
  }
  thread_id = 0;
  return r;
}

int ThreadBase::detach() { return pthread_detach(thread_id); }

int ThreadBase::kill(int signal) {
  if (thread_id) {
    return pthread_kill(thread_id, signal);
  } else {
    return -EINVAL;
  }
}

/* toolkits for std::thread */
void set_thread_name(std::thread &t, std::string &name) {
  int r = pthread_setname_np(t.native_handle(), name.c_str());
  if (r != 0) {
    throw std::system_error(r, std::generic_category(),
                            "failed to set thread name");
  }
}

std::string get_thread_name(const std::thread &t) {
  std::string name(16, '\0');
  // native_handle() is not const, but we only read the name, so use const_cast
  int r = pthread_getname_np(const_cast<std::thread &>(t).native_handle(),
                             name.data(), name.size());
  if (r != 0) {
    throw std::system_error(r, std::generic_category(),
                            "failed to get thread name");
  }
  name.resize(std::strlen(name.data()));
  return name;
}
void kill(std::thread &t, int signal) {
  int r = pthread_kill(t.native_handle(), signal);
  if (r != 0) {
    throw std::system_error(r, std::generic_category(),
                            "failed to kill thread");
  }
}