#include <functional>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <string_view>
#include <thread>
class ThreadBase {
  /*
  use pthread to implement more thread feartures than std::thread, .i.e.
  1. cpu affinity
  2. thread stack size
  3. signal handling
  4. thread priority
  5. thread name
  */
private:
  pid_t tid;
  pthread_t thread_id;
  std::string thread_name;
  int cpuid;
  void *entry_wrapper(); // wrapper some extra logic before calling entry()

private:
  static void *_entry_func(void *arg); // transform ThreadBase object to pthread
                                       // _start_routine parameter

protected:
  virtual void *entry() = 0; // real thread entry function

public:
  ThreadBase() : tid(0), thread_id(0), cpuid(-1){};
  virtual ~ThreadBase(){};

  ThreadBase(const ThreadBase &) = delete;
  ThreadBase &operator=(const ThreadBase &) = delete;

  const pthread_t &get_thread_id() const;
  pid_t get_tid() const;
  int set_cpu_affinity(int cpuid);
  int kill(int signal);
  int try_create(int stack_size);
  void create(const char *thread_name, int stack_size = 0);
  int join(void **retval = NULL);
  int detach();
};

// toolkits for std::thread
void set_thread_name(std::thread &t, std::string &name);
std::string get_thread_name(const std::thread &t);
void kill(std::thread &t, int signal);

template <typename Function, typename... Args>
std::thread make_named_thread(std::string_view n, Function &&func,
                              Args &&...args) {
  return std::thread(
      [n = std::string(n)](auto &&func, auto &&...args) {
        pthread_setname_np(pthread_self(), n.c_str());
        std::invoke(std::forward<Function>(func), std::forward<Args>(args)...);
      },
      std::forward<Function>(func), std::forward<Args>(args)...);
}