#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "synchronization/condition_variable.h"
#include "synchronization/mutex.h"
#include "synchronization/utils.h"

#include "heartbeat.h"
#include "thread.h"

class ThreadPool {
private:
  HeartbeatMap* hbmap;
  std::string name;
  std::string thread_name;
  std::string lock_name;
  BlueMutex _lock;
  BlueConditionVariable _cond;
  BlueConditionVariable _wait_cond;
  bool _stop;
  int _pause;
  int _draining;

public:
  using duration = std::chrono::duration<uint64_t, std::nano>;
  duration tp_grace;
  duration tp_suicide_grace;

  // ------------------------------------------------------------
  // thread pool handle
  class TPHandle {
    friend class ThreadPool;
    HeartbeatMap* hbmap;
    heartbeat_handle* hb;
    duration grace;
    duration suicide_grace;

  public:
    TPHandle(
        HeartbeatMap* hbmap,
        heartbeat_handle* hb,
        duration grace,
        duration suicide_grace) :
      hbmap(hbmap), hb(hb), grace(grace), suicide_grace(suicide_grace)
    {}

    void reset_tp_timeout();
    void suspent_tp_timeout();
  };

protected:
  unsigned _num_threads;
  int processing;

  // thread pool handle
  // ------------------------------------------------------------
  // work queue

public:
  class WorkQueue_ {
    // interface for work queue

  public:
    std::string name;
    duration grace;
    duration suicide_grace;

    WorkQueue_(std::string name, duration grace, duration suicide_grace) :
      name(std::move(name)), grace(grace), suicide_grace(suicide_grace)
    {}

    virtual ~WorkQueue_() {}

    virtual void _clear() = 0;
    virtual bool _empty() = 0;
    // must be thread safe
    virtual void* _void_dequeue() = 0;
    // catch global lock
    virtual void _void_process(void* item, TPHandle& handle) = 0;

    virtual void _void_process_finish(void*) = 0;
  };

protected:
  std::vector<WorkQueue_*> work_queues;
  int next_work_queue = 0;
  // work queue type
  // ------------------------------------------------------------
  // value-type work queue
  // ------------------------------------------------------------

public:
  template <typename T, typename U = T>
  class WorkQueueVal : public WorkQueue_ {
  private:
    BlueMutex _lock = BlueMutex(name + " WorkQueueVal::lock", false);
    ThreadPool* pool;
    std::list<U> to_process;
    std::list<U> to_finish;

    virtual void _enqueue(T) = 0;
    virtual void _enqueue_front(T) = 0;
    bool _empty() override = 0;
    virtual U _dequeue() = 0;

    virtual void
    _process_finish(U)
    {}

    void*
    _void_dequeue() override
    {
      {
        std::lock_guard<BlueMutex> l(_lock);
        if (_empty())
          return 0;
        U u = _dequeue();
        to_process.push_back(u);
      }
      return ((void*)1);
    }

    void
    _void_process(void* item, TPHandle& handle) override
    {
      _lock.lock();
      ASSERT(!to_process.empty());
      U u = to_process.front();
      to_process.pop_front();
      _lock.unlock();
      _process(u, handle);
      _lock.lock();
      to_finish.push_back(u);
      _lock.unlock();
    }

    void
    _void_process_finish(void* item) override
    {
      _lock.lock();
      ASSERT(!to_finish.empty());
      U u = to_finish.front();
      to_finish.pop_front();
      _lock.unlock();
      _process_finish(u);
    }

    void
    _clear() override
    {}

  protected:
    void
    lock()
    {
      pool->lock();
    }

    void
    unlock()
    {
      pool->unlock();
    }

    virtual void _process(U u, TPHandle&) = 0;

  public:
    WorkQueueVal(std::string n, duration g, duration sg, ThreadPool* p) :
      WorkQueue_(std::move(n), g, sg), pool(p)
    {
      pool->add_work_queue(this);
    }

    ~WorkQueueVal() override { pool->remove_work_queue(this); }

    void
    queue(T item)
    {
      std::lock_guard<BlueMutex> l(_lock);
      _enqueue(item);
      pool->_cond.notify_one();
    }

    void
    queue_front(T item)
    {
      std::lock_guard<BlueMutex> l(_lock);
      _enqueue_front(item);
      pool->_cond.notify_one();
    }

    void
    drain()
    {
      pool->drain(this);
    }
  };

  // value-type work queue
  // ------------------------------------------------------------

protected:
  // ------------------------------------------------------------
  // work thread
  class WorkThread : public ThreadBase {
  public:
    ThreadPool* pool;

    WorkThread(ThreadPool* p) : pool(p) {}

    void*
    entry() override
    {
      pool->worker(this);
      return 0;
    }
  };

  std::set<WorkThread*> _threads;
  std::list<WorkThread*> _old_threads;
  void start_threads();
  void join_old_threads();
  virtual void worker(WorkThread* wt);

public:
  ThreadPool(
      HeartbeatMap* hbmap,
      std::string nm,
      std::string tn,
      int n,
      duration grace,
      duration suicide_grace);
  ~ThreadPool();

  void
  lock()
  {
    _lock.lock();
  }

  void
  unlock()
  {
    _lock.unlock();
  }

  void
  wait(BlueConditionVariable& c)
  {
    std::unique_lock<BlueMutex> l(_lock, std::adopt_lock);
    c.wait(l);
  }

  void
  wake()
  {
    std::unique_lock<BlueMutex> l(_lock);
    _cond.notify_all();
  }

  void
  _wait()
  {
    std::unique_lock<BlueMutex> l(_lock, std::adopt_lock);
    _cond.wait(l);
  }

  void
  _wake()
  { // already have _lock
    _cond.notify_all();
  }

  void start();

  void stop();

  void pause();
  void unpause();

  void drain(WorkQueue_* wq);
  void add_work_queue(WorkQueue_* wq);
  void remove_work_queue(WorkQueue_* wq);
};
