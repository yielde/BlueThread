#include <atomic>
#include <mutex>
#include <thread>

class mutex_debug_base {
protected:
  std::string lock_name;
  int id = -1;
  bool lockdep;
  bool backtrace;
  std::atomic<int> locked_count;
  std::thread::id locked_by = {};
  bool _enable_lockdep() const { return lockdep; }

  mutex_debug_base(const std::string &name, bool lockdep = false,
                   bool backtrace = false);
  ~mutex_debug_base();

public:
};

class mutex_debug_impl: public mutex_debug_base {

  
};