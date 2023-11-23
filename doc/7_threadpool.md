# threadpool

分线程任务池管理类: threadpool。在构造函数中，已经生成对应数量的分线程用于处理任务队列中的任务。

> 可能有读者疑惑为什么获取数据连接动作放到threadpool模块中。请不要疑惑，原作者只是为了简单实现，方便理解。实际可以根据需要自己完成代码层级的优化。

> 另外本示例采用Proactor + ET(accept) 模式(主线程读取数据，分线程处理任务业务数据；accept 采用ET)，需要了解其他模式推荐自己尝试，或者找寻相关文档(原作TinyWebserver 对相关多种架构都做了尝试，请前往阅读)。

## 成员变量

```cpp
template <class T>
class threadpool {
public:
  threadpool(connection_pool* connPool, int thread_number = 8, int max_requests);
  ~threadpool();
  // 生产者加入任务方法
  bool append(T* request);

private:
  // 线程函数
  static void* worker(void* arg);
  // 线程执行函数
  void run();

private:
  int m_max_requests;           // 最大任务队列数
  pthread_t* m_threads;         // 线程池数组

  std::list<T*> m_workqueue;    // 任务队列

  locker m_mutex;               // 互斥锁
  sem m_sem;                    // 信号量
  connection_pool* m_connPool;  // 数据库连接池指针
}
```

## 核心方法

```cpp
// 构造函数，构造时创建任务线程
template <class T>
threadpool<T>::threadpool(connection_pool* connPool, int thread_number, int max_request): m_connPool(connPool), m_max_requests(max_requests) {
  if (thread_number <= 0 || max_requests <= 0)
    throw std::exception();
  m_threads = new pthread_t[thread_number];
  if (!m_threads)
    throw std::exception();
  for (int i = 0; i < thread_numbre; i++) {
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
// 析构函数
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
// 任务执行函数
// 当任务队列为空或者获取到的任务为空时，重新轮询任务
template <class T>
void threadpool<T>::run() {
  while (true) {
    // 信号量等待
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

    // 以上已取出任务对象，直接执行任务对象的process 方法
    connectionRAII mysqlcon(&request->mysql, m_connPool);
    request->process();
  }
}
```

当主线程获取完请求的数据并将request加入队列后，线程任务会通过信号量的等待解开，开始执行相关任务。

## 测试

请查看单元测试示例用法。
