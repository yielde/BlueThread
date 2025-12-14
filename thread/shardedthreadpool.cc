#include "shardedthreadpool.h"

#include <pthread.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>

#include "synchronization/mutex.h"
#include "synchronization/utils.h"
#include "thread/heartbeat.h"

ShardedThreadPool::ShardedThreadPool(
    HeartbeatMap* hbmap,
    std::string nm,
    std::string tn,
    uint32_t pnum_threads) :
  hbmp(hbmap),
  name(nm),
  lockname(name + "::lock"),
  thread_name(tn),
  shardedpool_lock(BlueMutex(lockname, false)),
  num_threads(pnum_threads),
  num_paused(0),
  num_drained(0),
  wq(NULL)
{}

void
ShardedThreadPool::start()
{
  std::cout << "starting ShardedThreadPool..." << std::endl;
  std::lock_guard<BlueMutex> l(shardedpool_lock);
  start_threads();
  std::cout << "started ShardedThreadPool" << std::endl;
}

void
ShardedThreadPool::stop()
{
  std::cout << "stopping ShardedThreadPool..." << std::endl;
  stop_threads = true;
  ASSERT(wq != NULL);
  wq->return_waiting_threads();
  for (auto p = threads_shardedpool.begin(); p != threads_shardedpool.end();
       ++p) {
    (*p)->join();
    delete *p;
  }
  threads_shardedpool.clear();
  std::cout << "stopped ShardedThreadPool..." << std::endl;
}

void
ShardedThreadPool::start_threads()
{
  ASSERT(shardedpool_lock.is_locked());
  uint32_t thread_index = 0;
  while (threads_shardedpool.size() < num_threads) {
    WorkThreadSharded* wt = new WorkThreadSharded(this, thread_index);
    threads_shardedpool.push_back(wt);
    wt->create(thread_name.c_str());
    std::cout << "started thread " << thread_name << " : " << pthread_self()
              << "with index " << thread_index << std::endl;
    thread_index++;
  }
}

void
ShardedThreadPool::start_shardedthreadpool_worker(uint32_t thread_index)
{
  ASSERT(wq != NULL);
  std::cout << "worker starting..." << std::endl;
  std::stringstream ss;
  ss << name << " thread " << (void*)pthread_self();
  auto hb = hbmp->add_worker(ss.str(), thread_index);

  while (!stop_threads) {
    if (pause_threads) {
      std::unique_lock<BlueMutex> ul(shardedpool_lock);
      ++num_paused;
      wait_cond.notify_all();
      while (pause_threads) {
        hbmp->reset_timeout(
            hb, wq->timeout_interval, wq->sucide_timeout_interval);
        shardedpool_cond.wait_for(
            ul, std::chrono::seconds(threadpool_empty_queue_max_wait_interval));
      }
      --num_paused;
    }
    if (drain_threads) {
      std::unique_lock<BlueMutex> ul(shardedpool_lock);
      if (wq->is_shard_empty(thread_index)) {
        ++num_drained;
        wait_cond.notify_all();
        while (drain_threads) {
          hbmp->reset_timeout(
              hb, wq->timeout_interval, wq->sucide_timeout_interval);
          shardedpool_cond.wait_for(
              ul,
              std::chrono::seconds(threadpool_empty_queue_max_wait_interval));
        }
        --num_drained;
      }
    }

    hbmp->reset_timeout(hb, wq->timeout_interval, wq->sucide_timeout_interval);
    wq->_process(thread_index, hb);
  }
  std::cout << "sharded worker finished" << std::endl;
  hbmp->remove_worker(hb);
}

void
ShardedThreadPool::pause()
{
  std::cout << "pausing" << std::endl;
  std::unique_lock<BlueMutex> ul(shardedpool_lock);
  pause_threads = true;
  wq->return_waiting_threads();
  while (num_threads != num_paused) {
    wait_cond.wait(ul);
  }
  std::cout << "paused" << std::endl;
}

void
ShardedThreadPool::unpause()
{
  std::cout << "unpausing" << std::endl;
  std::unique_lock<BlueMutex> ul(shardedpool_lock);
  pause_threads = false;
  wq->stop_return_waiting_threads();
  shardedpool_cond.notify_all();
  std::cout << "unpaused" << std::endl;
}

void
ShardedThreadPool::drain()
{
  std::unique_lock<BlueMutex> ul(shardedpool_lock);
  std::cout << "drain" << std::endl;
  drain_threads = true;
  ASSERT(wq != NULL);
  wq->return_waiting_threads();
  while (num_threads != num_drained) {
    wait_cond.wait(ul);
  }
  drain_threads = false;
  wq->stop_return_waiting_threads();
  shardedpool_cond.notify_all();
  std::cout << "drained" << std::endl;
}
