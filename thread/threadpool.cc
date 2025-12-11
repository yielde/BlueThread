#include "threadpool.h"

#include <pthread.h>

#include <chrono>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>

#include "synchronization/mutex.h"
#include "synchronization/utils.h"
#include "thread/heartbeat.h"

ThreadPool::ThreadPool(
    HeartbeatMap* hbmap,
    std::string nm,
    std::string tn,
    int n,
    duration grace,
    duration suicide_grace) :
  hbmap(hbmap),
  name(nm),
  thread_name(tn),
  lock_name(nm + "::lock"),
  _lock(BlueMutex(lock_name, false)),
  _stop(false),
  _pause(0),
  _draining(0),
  _num_threads(n),
  processing(0),
  tp_grace(grace),
  tp_suicide_grace(suicide_grace)
{}

ThreadPool::~ThreadPool() {}

void
ThreadPool::TPHandle::reset_tp_timeout()
{
  hbmap->reset_timeout(hb, grace, suicide_grace);
}

void
ThreadPool::TPHandle::suspent_tp_timeout()
{
  hbmap->clear_timeout(hb);
}

void
ThreadPool::start()
{
  std::cout << "ThreadPool::start..." << std::endl;
  _lock.lock();
  start_threads();
  _lock.unlock();
}

void
ThreadPool::stop()
{
  std::cout << "ThreadPool::stop..." << std::endl;
  _lock.lock();
  _stop = true;
  _cond.notify_all();
  join_old_threads();
  _lock.unlock();
  for (auto p = _threads.begin(); p != _threads.end(); ++p) {
    (*p)->join();
    delete *p;
  }
  _threads.clear();
  _lock.lock();

  for (unsigned i = 0; i < work_queues.size(); ++i) {
    work_queues[i]->_clear();
  }
  _stop = false;
  _lock.unlock();
  std::cout << "ThreadPool::stop stoped!" << std::endl;
}

void
ThreadPool::start_threads()
{
  ASSERT(_lock.is_locked());
  while (_threads.size() < _num_threads) {
    WorkThread* wt = new WorkThread(this);
    _threads.insert(wt);
    wt->create(thread_name.c_str());
    std::cout << "ThreadPool::start_threads: created thread: "
              << wt->get_thread_id() << std::endl;
  }
}

void
ThreadPool::join_old_threads()
{
  ASSERT(_lock.is_locked());
  while (!_old_threads.empty()) {
    _old_threads.front()->join();
    delete _old_threads.front();
    _old_threads.pop_front();
  }
}

void
ThreadPool::add_work_queue(WorkQueue_* wq)
{
  std::lock_guard<BlueMutex> l(_lock);
  work_queues.push_back(wq);
}

void
ThreadPool::remove_work_queue(WorkQueue_* wq)
{
  std::lock_guard<BlueMutex> l(_lock);
  auto it = std::find(work_queues.begin(), work_queues.end(), wq);
  if (it != work_queues.end()) {
    work_queues.erase(it);
  }
}

void
ThreadPool::pause()
{
  std::cout << "pausing" << std::endl;
  std::unique_lock<BlueMutex> ul(_lock);
  _pause++;
  while (processing) {
    _wait_cond.wait(ul);
  }
  std::cout << "paused" << std::endl;
}

void
ThreadPool::unpause()
{
  std::unique_lock<BlueMutex> ul(_lock);
  std::cout << "unpause" << std::endl;
  _pause--;
  _cond.notify_all();
}

void
ThreadPool::drain(WorkQueue_* wq)
{

  std::unique_lock<BlueMutex> ul(_lock);
  std::cout << "draining" << std::endl;
  _draining++;
  while (processing || (wq != nullptr && !wq->_empty())) {
    _wait_cond.wait(ul);
  }
  _draining--;
}

void
ThreadPool::worker(WorkThread* wt)
{
  std::unique_lock<BlueMutex> ul(_lock);
  std::stringstream ss;
  ss << name << " thread " << (void*)pthread_self();
  auto hb = hbmap->add_worker(ss.str(), pthread_self());
  while (!_stop) {
    join_old_threads();
    if (work_queues.empty()) {
      std::cout << "worker no work queues" << std::endl;
    } else if (!_pause) {
      WorkQueue_* wq;
      int tries = 2 * work_queues.size();
      bool did = false;
      while (tries--) {
        next_work_queue %= work_queues.size();
        wq = work_queues[next_work_queue++];
        void* item = wq->_void_dequeue();
        if (item) {
          processing++;
          std::cout << "worker wq " << wq->name << " start processing " << item
                    << "( " << processing << " active) " << std::endl;
          ul.unlock();
          TPHandle tp_handle(hbmap, hb, wq->grace, wq->suicide_grace);
          tp_handle.reset_tp_timeout();
          wq->_void_process(item, tp_handle);
          ul.lock();
          wq->_void_process_finish(item);
          processing--;
          std::cout << "worker wq " << wq->name << " done processing " << item
                    << "( " << processing << " active) " << std::endl;
          if (_pause || _draining) {
            _wait_cond.notify_all();
          }
          did = true;
          break;
        }
      }
      if (did)
        continue;
    }
    std::cout << "worker waiting" << std::endl;
    hbmap->reset_timeout(hb, tp_grace, tp_suicide_grace);
    auto wait = std::chrono::seconds(10);
    _cond.wait_for(ul, wait);
  }
  std::cout << "worker finished: " << ss.str() << std::endl;
  hbmap->remove_worker(hb);
}
