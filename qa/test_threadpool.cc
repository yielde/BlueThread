#include "thread/threadpool.h"
#include "thread/heartbeat.h"
#include <chrono>
#include <iostream>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

using namespace std;

// 测试用的工作项类型
struct TestTask {
  int id;
  int value;
  TestTask(int id, int value) : id(id), value(value) {}
};

// 统计处理结果
class TaskResult {
public:
  mutex mtx;
  vector<int> processed_ids;
  int total_processed = 0;

  void add_processed(int id) {
    lock_guard<mutex> lock(mtx);
    processed_ids.push_back(id);
    total_processed++;
  }

  void print_results() {
    lock_guard<mutex> lock(mtx);
    cout << "Total processed: " << total_processed << endl;
    cout << "Processed IDs: ";
    for (int id : processed_ids) {
      cout << id << " ";
    }
    cout << endl;
  }
};

// 实现一个具体的 WorkQueueVal 类
// WorkQueueVal 现在有构造函数，可以正确初始化基类 WorkQueue_
class TestWorkQueue : public ThreadPool::WorkQueueVal<TestTask> {
private:
  list<TestTask> queue;
  TaskResult *result;
  BlueMutex queue_lock;  // 自己的锁，用于保护队列操作

public:
  TestWorkQueue(const string &name, ThreadPool::duration grace,
                ThreadPool::duration suicide_grace, TaskResult *result)
      : ThreadPool::WorkQueueVal<TestTask>(name, grace, suicide_grace),
        result(result), queue_lock(name + "::queue_lock", true) {}

  void _enqueue(TestTask task) override {
    lock_guard<BlueMutex> lock(queue_lock);
    queue.push_back(task);
    cout << "Enqueued task id=" << task.id << " value=" << task.value << endl;
  }

  void _enqueue_front(TestTask task) override {
    lock_guard<BlueMutex> lock(queue_lock);
    queue.push_front(task);
    cout << "Enqueued front task id=" << task.id << " value=" << task.value
         << endl;
  }

  bool _empty() override {
    lock_guard<BlueMutex> lock(queue_lock);
    return queue.empty();
  }

  TestTask _dequeue() override {
    lock_guard<BlueMutex> lock(queue_lock);
    ASSERT(!queue.empty());
    TestTask task = queue.front();
    queue.pop_front();
    return task;
  }

  void _process(TestTask task, ThreadPool::TPHandle &handle) override {
    cout << "Processing task id=" << task.id << " value=" << task.value
         << " in thread " << pthread_self() << endl;
    // 模拟一些处理工作
    this_thread::sleep_for(chrono::milliseconds(10));
    // 重置超时
    handle.reset_tp_timeout();
    // 记录处理结果
    if (result) {
      result->add_processed(task.id);
    }
  }

  void _process_finish(TestTask task) override {
    cout << "Finished processing task id=" << task.id << endl;
  }

  void _clear() override {
    lock_guard<BlueMutex> lock(queue_lock);
    queue.clear();
  }
};

// 测试基本功能
void test_basic_workqueue() {
  cout << "\n=== Test Basic WorkQueue ===" << endl;

  HeartbeatMap hbmap("test_pool");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(10);

  ThreadPool pool(&hbmap, "test_pool", "test_thread", 2, grace, suicide_grace);

  TaskResult result;
  TestWorkQueue workqueue("test_queue", grace, suicide_grace, &result);

  pool.add_work_queue(&workqueue);
  pool.start();

  // 等待线程池启动
  this_thread::sleep_for(chrono::milliseconds(100));

  // 添加一些任务
  cout << "\nAdding tasks..." << endl;
  for (int i = 1; i <= 10; i++) {
    workqueue._enqueue(TestTask(i, i * 10));
    this_thread::sleep_for(chrono::milliseconds(50));
  }

  // 等待所有任务处理完成
  cout << "\nWaiting for tasks to complete..." << endl;
  pool.drain(&workqueue);

  result.print_results();

  pool.stop();
  cout << "Test Basic WorkQueue completed\n" << endl;
}

// 测试前端入队
void test_enqueue_front() {
  cout << "\n=== Test Enqueue Front ===" << endl;

  HeartbeatMap hbmap("test_pool2");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(10);

  ThreadPool pool(&hbmap, "test_pool2", "test_thread2", 2, grace,
                  suicide_grace);

  TaskResult result;
  TestWorkQueue workqueue("test_queue2", grace, suicide_grace, &result);

  pool.add_work_queue(&workqueue);
  pool.start();

  this_thread::sleep_for(chrono::milliseconds(100));

  // 先添加一些普通任务
  cout << "\nAdding normal tasks..." << endl;
  for (int i = 1; i <= 5; i++) {
    workqueue._enqueue(TestTask(i, i * 10));
  }

  // 然后添加一些前端任务（应该优先处理）
  cout << "\nAdding front tasks..." << endl;
  for (int i = 10; i <= 12; i++) {
    workqueue._enqueue_front(TestTask(i, i * 10));
  }

  pool.drain(&workqueue);

  result.print_results();

  pool.stop();
  cout << "Test Enqueue Front completed\n" << endl;
}

// 测试暂停和恢复
void test_pause_unpause() {
  cout << "\n=== Test Pause/Unpause ===" << endl;

  HeartbeatMap hbmap("test_pool3");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(10);

  ThreadPool pool(&hbmap, "test_pool3", "test_thread3", 2, grace,
                  suicide_grace);

  TaskResult result;
  TestWorkQueue workqueue("test_queue3", grace, suicide_grace, &result);

  pool.add_work_queue(&workqueue);
  pool.start();

  this_thread::sleep_for(chrono::milliseconds(100));

  // 添加一些任务
  cout << "\nAdding tasks..." << endl;
  for (int i = 1; i <= 5; i++) {
    workqueue._enqueue(TestTask(i, i * 10));
  }

  // 暂停线程池
  cout << "\nPausing thread pool..." << endl;
  pool.pause();

  // 在暂停状态下添加更多任务
  cout << "Adding more tasks while paused..." << endl;
  for (int i = 6; i <= 10; i++) {
    workqueue._enqueue(TestTask(i, i * 10));
  }

  // 等待一下，确保没有任务被处理
  this_thread::sleep_for(chrono::milliseconds(200));
  cout << "Tasks processed while paused: " << result.total_processed << endl;

  // 恢复线程池
  cout << "\nUnpausing thread pool..." << endl;
  pool.unpause();

  // 等待所有任务完成
  pool.drain(&workqueue);

  result.print_results();

  pool.stop();
  cout << "Test Pause/Unpause completed\n" << endl;
}

// 测试多线程并发
void test_concurrent_tasks() {
  cout << "\n=== Test Concurrent Tasks ===" << endl;

  HeartbeatMap hbmap("test_pool4");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(10);

  ThreadPool pool(&hbmap, "test_pool4", "test_thread4", 4, grace,
                  suicide_grace);

  TaskResult result;
  TestWorkQueue workqueue("test_queue4", grace, suicide_grace, &result);

  pool.add_work_queue(&workqueue);
  pool.start();

  this_thread::sleep_for(chrono::milliseconds(100));

  // 从多个线程并发添加任务
  cout << "\nAdding tasks from multiple threads..." << endl;
  vector<thread> threads;
  for (int t = 0; t < 3; t++) {
    threads.emplace_back([&workqueue, t]() {
      for (int i = 1; i <= 5; i++) {
        int id = t * 10 + i;
        workqueue._enqueue(TestTask(id, id * 10));
        this_thread::sleep_for(chrono::milliseconds(20));
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  // 等待所有任务完成
  pool.drain(&workqueue);

  result.print_results();

  pool.stop();
  cout << "Test Concurrent Tasks completed\n" << endl;
}

int main(int argc, char **argv) {
  cout << "Starting ThreadPool and WorkQueueVal tests..." << endl;

  test_basic_workqueue();
  test_enqueue_front();
  test_pause_unpause();
  test_concurrent_tasks();

  cout << "All tests completed!" << endl;
  return 0;
}

