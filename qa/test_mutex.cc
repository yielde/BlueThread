#include "synchronization/mutex.h"
#include "synchronization/mutex_recursive.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace std;

static void test_lock(){
    BlueMutex mutex("test_mutex", true);
    mutex.lock();
    cout << "locked" << endl;
    mutex.unlock();
    cout << "unlocked" << endl;
}

static void test_lock_recursive(mutex_recursive &mutex_recursive) {
  pthread_t tid = pthread_self();
  mutex_recursive.lock();
  cout << "locked 1 by thread " << tid << endl;
  mutex_recursive.lock();
  cout << "locked 2 by thread " << tid << endl;
  mutex_recursive.unlock();
  cout << "unlocked 2 by thread " << tid << endl;
  mutex_recursive.unlock();
  cout << "unlocked 1 by thread " << tid << endl;
}

static void test_lock_recursive_thread() {
  pthread_t tid = pthread_self();
  mutex_recursive mutex_recursive;
  thread t(test_lock_recursive, std::ref(mutex_recursive));
  mutex_recursive.lock();
  cout << "locked 3 by thread " << tid << endl;
  mutex_recursive.unlock();
  cout << "unlocked 3 by thread " << tid << endl;
  t.join();
}

static int test_no_lock_thread(int count) {
  thread t;
  BlueMutex mutex("test_lock_thread", false);
  // 将操作分解为读取-修改-写入，增加竞争概率
  t = thread(
      [](int &count) {
        for (int i = 0; i < 1000; i++) {
          int temp = count; // 读取
          temp = temp + 1;  // 修改（故意分解操作）
          count = temp;     // 写入
        }
      },
      std::ref(count));
  thread t2 = thread(
      [](int &count) {
        for (int i = 0; i < 1000; i++) {
          int temp = count; // 读取
          temp = temp - 1;  // 修改（故意分解操作）
          count = temp;     // 写入
        }
      },
      std::ref(count));
  t.join();
  t2.join();
  return count;
}

static int test_lock_thread(int count) {
  thread t;
  BlueMutex mutex("test_lock_thread", false);
  // 将操作分解为读取-修改-写入，增加竞争概率
  t = thread(
      [&mutex](int &count) {
        for (int i = 0; i < 1000; i++) {
          mutex.lock();
          int temp = count; // 读取
          temp = temp + 1;  // 修改（故意分解操作）
          count = temp;     // 写入
          mutex.unlock();
        }
      },
      std::ref(count));
  thread t2 = thread(
      [&mutex](int &count) {
        for (int i = 0; i < 1000; i++) {
          mutex.lock();
          int temp = count; // 读取
          temp = temp - 1;  // 修改（故意分解操作）
          count = temp;     // 写入
          mutex.unlock();
        }
      },
      std::ref(count));
  t.join();
  t2.join();
  cout << "final count: " << count << endl;
  return count;
}

static void test_lock_thread_main() {
  int wrong_count = 0;
  int total_tests = 100;
  int initial_value = 100;

  cout << "Testing data race without lock..." << endl;
  cout << "Initial value: " << initial_value << endl;
  cout << "Expected final value: " << initial_value
       << " (10000 increments - 10000 decrements)" << endl;
  cout << "Running " << total_tests << " tests..." << endl;

  for (int i = 0; i < total_tests; i++) {
    int result = test_lock_thread(initial_value);
    if (result != initial_value) {
      wrong_count++;
      cout << "  Test " << i << ": WRONG! Got " << result << " instead of "
           << initial_value << endl;
    }
  }

  cout << "\nSummary:" << endl;
  cout << "  Total tests: " << total_tests << endl;
  cout << "  Wrong results: " << wrong_count << endl;
  cout << "  Correct results: " << (total_tests - wrong_count) << endl;
  cout << "  Error rate: " << (wrong_count * 100.0 / total_tests) << "%"
       << endl;
}

static void test_no_lock_thread_main() {
  int wrong_count = 0;
  int total_tests = 100;
  int initial_value = 100;

  cout << "Testing data race without lock..." << endl;
  cout << "Initial value: " << initial_value << endl;
  cout << "Expected final value: " << initial_value
       << " (10000 increments - 10000 decrements)" << endl;
  cout << "Running " << total_tests << " tests..." << endl;

  for (int i = 0; i < total_tests; i++) {
    int result = test_no_lock_thread(initial_value);
    if (result != initial_value) {
      wrong_count++;
      cout << "  Test " << i << ": WRONG! Got " << result << " instead of "
           << initial_value << endl;
    }
  }

  cout << "\nSummary:" << endl;
  cout << "  Total tests: " << total_tests << endl;
  cout << "  Wrong results: " << wrong_count << endl;
  cout << "  Correct results: " << (total_tests - wrong_count) << endl;
  cout << "  Error rate: " << (wrong_count * 100.0 / total_tests) << "%"
       << endl;
}

void test_deadlock() {
  BlueMutex mutex1("mutex1", true);
  BlueMutex mutex2("mutex2", true);
  mutex1.lock();
  mutex1.lock();
  mutex2.lock();
  cout << "locked mutex1 and mutex2" << endl;
  mutex1.unlock();
  mutex2.unlock();
  cout << "unlocked mutex1 and mutex2" << endl;
}

void test_deadlock_thread(BlueMutex &mutex, BlueMutex &will_mutex) {
  mutex.lock();
  cout << "locked mutex by thread " << pthread_self() << " " << mutex.get_name()
       << endl;
  sleep(1);
  will_mutex.lock();
  cout << "locked will_mutex by thread " << pthread_self() << " "
       << will_mutex.get_name() << endl;
  mutex.unlock();
  cout << "unlocked mutex by thread " << pthread_self() << " "
       << mutex.get_name() << endl;
  will_mutex.unlock();
  cout << "unlocked will_mutex by thread " << pthread_self() << " "
       << will_mutex.get_name() << endl;
}

void test_deadlock_thread_main() {
  BlueMutex mutex1("mutex0", true);
  BlueMutex mutex2("mutex1", true);
  BlueMutex mutex3("mutex2", true);
  thread t1(test_deadlock_thread, std::ref(mutex1), std::ref(mutex2));
  thread t2(test_deadlock_thread, std::ref(mutex2), std::ref(mutex3));
  thread t3(test_deadlock_thread, std::ref(mutex3), std::ref(mutex1));
  t1.join();
  t2.join();
  t3.join();
}

int main(int argc, char **argv) {
  //   test_lock();
  // test_lock_recursive_thread();
  // test_lock_thread(100);
  // test_lock_thread_main();
  // test_no_lock_thread_main();
  test_deadlock_thread_main();
  return 0;
}