#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include "thread/heartbeat.h"
#include "thread/threadpool.h"

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

  void
  add_processed(int id)
  {
    lock_guard<mutex> lock(mtx);
    processed_ids.push_back(id);
    total_processed++;
  }

  void
  print_results()
  {
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
// WorkQueueVal 构造函数需要 ThreadPool* 参数，并会自动添加到线程池
class TestWorkQueue : public ThreadPool::WorkQueueVal<TestTask> {
private:
  list<TestTask> task_queue; // 重命名以避免与基类的 queue() 方法冲突
  TaskResult* result;

public:
  TestWorkQueue(
      const string& name,
      ThreadPool::duration grace,
      ThreadPool::duration suicide_grace,
      ThreadPool* pool,
      TaskResult* result) :
    ThreadPool::WorkQueueVal<TestTask>(name, grace, suicide_grace, pool),
    result(result)
  {}

  void
  _enqueue(TestTask task) override
  {
    task_queue.push_back(task);
    cout << "Enqueued task id=" << task.id << " value=" << task.value << endl;
  }

  void
  _enqueue_front(TestTask task) override
  {
    task_queue.push_front(task);
    cout << "Enqueued front task id=" << task.id << " value=" << task.value
         << endl;
  }

  bool
  _empty() override
  {
    return task_queue.empty();
  }

  TestTask
  _dequeue() override
  {
    ASSERT(!task_queue.empty());
    TestTask task = task_queue.front();
    task_queue.pop_front();
    return task;
  }

  void
  _process(TestTask task, ThreadPool::TPHandle& handle) override
  {
    cout << "Processing task id=" << task.id << " value=" << task.value
         << " in thread " << pthread_self() << endl;
    // 模拟一些处理工作
    this_thread::sleep_for(chrono::milliseconds(2000));
    // 记录处理结果
    if (result) {
      result->add_processed(task.id);
    }
  }

  void
  _process_finish(TestTask task) override
  {
    cout << "Finished processing task id=" << task.id << endl;
  }

  void
  _clear() override
  {
    task_queue.clear();
  }
};

// 测试基本功能
TEST(ThreadPoolTest, BasicWorkQueue)
{
  HeartbeatMap hbmap("test_pool");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(15);

  ThreadPool pool(&hbmap, "test_pool", "test_thread", 10, grace, suicide_grace);

  TaskResult result;
  // WorkQueueVal 构造函数会自动将 work queue 添加到线程池
  TestWorkQueue workqueue("test_queue", grace, suicide_grace, &pool, &result);

  pool.start();

  // 使用公共方法 queue() 添加任务
  const int num_tasks = 10;
  for (int i = 1; i <= num_tasks; i++) {
    workqueue.queue(TestTask(i, i * 10));
    this_thread::sleep_for(chrono::milliseconds(50));
  }

  // 等待所有任务处理完成
  workqueue.drain();

  // 验证所有任务都被处理了
  EXPECT_EQ(result.total_processed, num_tasks);
  EXPECT_EQ(result.processed_ids.size(), static_cast<size_t>(num_tasks));

  // 验证任务 ID 正确
  for (int i = 1; i <= num_tasks; i++) {
    EXPECT_TRUE(
        find(result.processed_ids.begin(), result.processed_ids.end(), i) !=
        result.processed_ids.end());
  }

  pool.stop();
  // workqueue 析构函数会自动从线程池中移除
}

// 测试前端入队
TEST(ThreadPoolTest, EnqueueFront)
{
  HeartbeatMap hbmap("test_pool2");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(10);

  ThreadPool pool(&hbmap, "test_pool2", "test_thread2", 2, grace, suicide_grace);

  TaskResult result;
  TestWorkQueue workqueue("test_queue2", grace, suicide_grace, &pool, &result);

  pool.start();

  this_thread::sleep_for(chrono::milliseconds(100));

  // 先添加一些普通任务
  for (int i = 1; i <= 5; i++) {
    workqueue.queue(TestTask(i, i * 10));
  }

  // 然后添加一些前端任务（应该优先处理）
  for (int i = 10; i <= 12; i++) {
    workqueue.queue_front(TestTask(i, i * 10));
  }

  workqueue.drain();

  // 验证所有任务都被处理了
  EXPECT_EQ(result.total_processed, 8); // 5 + 3 = 8

  pool.stop();
}

// 测试暂停和恢复
TEST(ThreadPoolTest, PauseUnpause)
{
  HeartbeatMap hbmap("test_pool3");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(10);

  ThreadPool pool(&hbmap, "test_pool3", "test_thread3", 2, grace, suicide_grace);

  TaskResult result;
  TestWorkQueue workqueue("test_queue3", grace, suicide_grace, &pool, &result);

  pool.start();

  // 添加一些任务
  for (int i = 1; i <= 5; i++) {
    workqueue.queue(TestTask(i, i * 10));
  }

  // 暂停线程池
  pool.pause();

  // 在暂停状态下添加更多任务
  for (int i = 6; i <= 10; i++) {
    workqueue.queue(TestTask(i, i * 10));
  }

  // 等待一下，确保暂停期间任务不会被处理
  this_thread::sleep_for(chrono::milliseconds(200));
  int processed_while_paused = result.total_processed;

  // 恢复线程池
  pool.unpause();

  // 等待所有任务完成
  workqueue.drain();

  // 验证暂停期间没有或很少任务被处理（由于任务处理时间2秒，暂停200ms应该处理很少）
  // 验证最终所有任务都被处理了
  EXPECT_EQ(result.total_processed, 10);

  pool.stop();
}

// 测试多线程并发
TEST(ThreadPoolTest, ConcurrentTasks)
{
  HeartbeatMap hbmap("test_pool4");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(10);

  ThreadPool pool(&hbmap, "test_pool4", "test_thread4", 4, grace, suicide_grace);

  TaskResult result;
  TestWorkQueue workqueue("test_queue4", grace, suicide_grace, &pool, &result);

  pool.start();

  this_thread::sleep_for(chrono::milliseconds(100));

  // 从多个线程并发添加任务
  const int num_producer_threads = 3;
  const int tasks_per_thread = 5;
  vector<thread> threads;
  for (int t = 0; t < num_producer_threads; t++) {
    threads.emplace_back([&workqueue, t]() {
      for (int i = 1; i <= tasks_per_thread; i++) {
        int id = t * 10 + i;
        workqueue.queue(TestTask(id, id * 10));
        this_thread::sleep_for(chrono::milliseconds(20));
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // 等待所有任务完成
  workqueue.drain();

  // 验证所有任务都被处理了
  const int expected_total = num_producer_threads * tasks_per_thread;
  EXPECT_EQ(result.total_processed, expected_total);

  // 验证任务 ID 正确
  for (int t = 0; t < num_producer_threads; t++) {
    for (int i = 1; i <= tasks_per_thread; i++) {
      int id = t * 10 + i;
      EXPECT_TRUE(
          find(result.processed_ids.begin(), result.processed_ids.end(), id) !=
          result.processed_ids.end());
    }
  }

  pool.stop();
}

// 测试多个工作队列
TEST(ThreadPoolTest, MultipleWorkQueues)
{
  HeartbeatMap hbmap("test_pool5");
  ThreadPool::duration grace = chrono::seconds(5);
  ThreadPool::duration suicide_grace = chrono::seconds(10);

  ThreadPool pool(&hbmap, "test_pool5", "test_thread5", 3, grace, suicide_grace);

  TaskResult result1, result2;
  TestWorkQueue workqueue1(
      "test_queue5_1", grace, suicide_grace, &pool, &result1);
  TestWorkQueue workqueue2(
      "test_queue5_2", grace, suicide_grace, &pool, &result2);

  pool.start();

  this_thread::sleep_for(chrono::milliseconds(100));

  // 向两个队列添加任务
  const int tasks_queue1 = 5;
  const int tasks_queue2 = 6;
  for (int i = 1; i <= tasks_queue1; i++) {
    workqueue1.queue(TestTask(i, i * 10));
  }

  for (int i = 10; i <= 15; i++) {
    workqueue2.queue(TestTask(i, i * 10));
  }

  // 等待所有任务完成
  workqueue1.drain();
  workqueue2.drain();

  // 验证两个队列的任务都被正确处理
  EXPECT_EQ(result1.total_processed, tasks_queue1);
  EXPECT_EQ(result2.total_processed, tasks_queue2);

  // 验证队列1的任务ID
  for (int i = 1; i <= tasks_queue1; i++) {
    EXPECT_TRUE(
        find(result1.processed_ids.begin(), result1.processed_ids.end(), i) !=
        result1.processed_ids.end());
  }

  // 验证队列2的任务ID
  for (int i = 10; i <= 15; i++) {
    EXPECT_TRUE(
        find(result2.processed_ids.begin(), result2.processed_ids.end(), i) !=
        result2.processed_ids.end());
  }

  pool.stop();
}

// main 函数由 gtest_main 自动提供，无需手动定义
