#include <pthread.h>

#include "synchronization/condition_variable.h"
#include "synchronization/mutex.h"
#include "thread/thread.h"

class TestThread : public ThreadBase {
public:
  int& i;
  condition_variable& cond;
  BlueMutex& mutex;

  TestThread(int& i, condition_variable& cond, BlueMutex& mutex) :
    i(i), cond(cond), mutex(mutex)
  {}

  // 此函数负责对外部的变量进行修改
  void*
  entry() override
  {
    while (i < 10) {
      mutex.lock();
      std::cout << "TestThread【" << pthread_self() << "】 i: "
                << " " << i << std::endl;
      i++;
      cond.notify_one();
      mutex.unlock();
    }
    return nullptr;
  }
};

int
main()
{
  BlueMutex mutex("test_cond", true);
  condition_variable cond(&mutex);
  int i = 0;
  TestThread thread(i, cond, mutex);
  thread.create("test_thread");

  std::unique_lock<BlueMutex> lock(mutex);
  cond.wait(lock, [&]() {
    if (i < 10) {
      std::cout << "wait for i == 0" << std::endl;
      return false;
    } else {
      std::cout << "i == 10" << std::endl;
      return true;
    }
  });
  thread.join();
  std::cout << "i: " << i << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return 0;
}