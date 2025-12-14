#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <ratio>
#include <string>
#include <vector>

#include "synchronization/condition_variable.h"
#include "synchronization/mutex.h"

#include "heartbeat.h"
#include "thread.h"

class ShardedThreadPool {
public:
  using clock = std::chrono::steady_clock;
  using duration = std::chrono::duration<std::uint64_t, std::nano>;

private:
  HeartbeatMap* hbmp;
  std::string name;
  std::string thread_name;
  std::string lockname;
  BlueMutex shardedpool_lock;
  BlueConditionVariable shardedpool_cond;
  BlueConditionVariable wait_cond;
  uint32_t num_threads;

  std::atomic<bool> stop_threads = {false};
  std::atomic<bool> pause_threads = {false};
  std::atomic<bool> drain_threads = {false};
  uint32_t num_paused;
  uint32_t num_drained;

  uint32_t threadpool_empty_queue_max_wait_interval;

public:
  class BaseShardedWQ {
  public:
    duration timeout_interval;
    duration sucide_timeout_interval;

    BaseShardedWQ(duration ti, duration sti) :
      timeout_interval(ti), sucide_timeout_interval(sti)
    {}

    virtual ~BaseShardedWQ() {}

    virtual void _process(uint32_t thread_index, heartbeat_handle* hb) = 0;
    virtual void return_waiting_threads() = 0;
    virtual void stop_return_waiting_threads() = 0;
    virtual bool is_shard_empty(uint32_t thread_index) = 0;
  };

  template <typename T>
  class ShardedWQ : public BaseShardedWQ {
    ShardedThreadPool* sharded_pool;

  protected:
    virtual void _enqueue(T&&) = 0;
    virtual void _enqueue_front(T&&) = 0;

  public:
    ShardedWQ(duration ti, duration sti, ShardedThreadPool* tp) :
      BaseShardedWQ(ti, sti), sharded_pool(tp)
    {
      tp->set_wq(this);
    }

    ~ShardedWQ() override {}

    void
    queue(T&& item)
    {
      _enqueue(std::move(item));
    }

    void
    queue_front(T&& item)
    {
      _enqueue_front(item);
    }

    void
    drain()
    {
      sharded_pool->drain();
    }
  };

private:
  BaseShardedWQ* wq;

  class WorkThreadSharded : public ThreadBase {
  public:
    ShardedThreadPool* pool;
    uint32_t thread_index;

    WorkThreadSharded(ShardedThreadPool* tp, uint32_t pthread_index) :
      pool(tp), thread_index(pthread_index)
    {}

    void*
    entry() override
    {
      pool->start_shardedthreadpool_worker(thread_index);
      return 0;
    }
  };

  std::vector<WorkThreadSharded*> threads_shardedpool;
  void start_threads();
  void start_shardedthreadpool_worker(uint32_t thread_index);

  void
  set_wq(BaseShardedWQ* wq)
  {
    this->wq = wq;
  }

public:
  ShardedThreadPool(
      HeartbeatMap* hbmap,
      std::string nm,
      std::string tn,
      uint32_t pnum_threads);
  void start();
  void stop();
  void pause();
  void unpause();
  void drain();
};
