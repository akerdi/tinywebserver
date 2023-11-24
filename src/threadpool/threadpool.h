#pragma once

#include <list>

#include "../lock/lock.h"
#include "../CGImysql/sql_connection_pool.h"

template <class T>
class threadpool {
public:
  threadpool(connection_pool* connPool, int thread_number = 8, int max_requests = 10000);
  ~threadpool();
  bool append(T* request);

private:
  static void* worker(void* arg);
  void run();

private:
  int m_max_requests;
  pthread_t* m_threads;
  std::list<T*> m_workqueue;
  locker m_mutex;
  sem m_sem;
  connection_pool* m_connPool;
};

template <class T>
threadpool<T>::threadpool(connection_pool* connPool, int thread_number, int max_requests):m_connPool(connPool),m_max_requests(max_requests) {
  if (thread_number <= 0 || max_requests <= 0)
    throw std::exception();
  m_threads = new pthread_t[thread_number];
  if (!m_threads)
    throw std::exception();
  for (int i = 0; i < thread_number; i++) {
    if (pthread_create(m_threads + i, NULL, threadpool::worker, this)) {
      delete [] m_threads;
      throw std::exception();
    }
    if (pthread_detach(m_threads[i])) {
      delete [] m_threads;
      throw std::exception();
    }
  }
}
template <class T>
bool threadpool<T>::append(T* request) {
  m_mutex.lock();
  if (m_workqueue.size() >= m_max_requests) {
    m_mutex.unlock();
    return false;
  }
  m_workqueue.push_back(request);
  m_mutex.unlock();

  m_sem.post();
  return true;
}
template <class T>
threadpool<T>::~threadpool() {
  if (m_threads != NULL)
    delete [] m_threads;
}
template <class T>
void* threadpool<T>::worker(void* arg) {
  threadpool<T>* that = (threadpool<T>*)arg;
  that->run();
}

template <class T>
void threadpool<T>::run() {
  while (true) {
    m_sem.wait();

    m_mutex.lock();
    if (m_workqueue.empty()) {
      m_mutex.unlock();
      continue;
    }
    T* request = m_workqueue.front();
    m_workqueue.pop_front();
    m_mutex.unlock();

    if (!request) {
      continue;
    }
    connectionRAII mysqlcon(&request->mysql, m_connPool);
    request->process();
  }
}
