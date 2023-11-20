# block_queue

block_queue 阻塞队列封装了生产者-消费者模型，其中push成员是生产者，pop成员是消费者。

阻塞队列中，使用了循环数组作为队列，使用数组下标作为数据内容容量。如 front0 - front 1 - back0 - back1, front0 - back0为当前数据容量，当生产数据至back1时，m_back 则为已有数据的下标索引；当消费数据至front1时，m_front 则为消费数据的下标索引。

## 成员变量

```cpp
template <class T> class block_queue {
private:
  locker m_mutex; // 阻塞锁
  cond m_cond;    // 等待wait 条件锁

  T* m_array;     // 队列中数据指针
  int m_max_size; // 设置队列大小
  int m_size;     // 当前内容大小
  int m_front;    // 消费者索引下标
  int m_back;     // 生产者索引下标
};
```

以上成员变量m_mutex 负责读取/修改成员变量时的加/减锁，m_cond 条件锁用于获取读取数据时的超时机制(没有数据则执行等待/超时等待的机制)。余下是操作数据的变量，如`T* m_array` 为操作的数据队列。

## 成员函数

成员函数操作变量时，都应短暂性加锁防止线程间竞抢:

```cpp
template <class T> class block_queue {
  ...
  bool full() {
    m_mutex.lock();
    if (m_size >= m_max_size) {
      m_mutex.unlock();
      return true;
    }
    m_mutex.unlock();
    return false;
  }
  int size() {
    int tmp = 0;
    m_mutex.lock();
    tmp = m_size;
    m_mutex.unlock();
    return tmp;
  }
```

以上可以看到`bool(*fool)()` 方法中判别变量的加/减锁方式，对比`int(*size)()` 方法看到操作局部变量方案。

核心方法是生产者/消费者 的三个方法:

```cpp
template <class T> class block_queue {
  ...
  bool push(const T& item) {
    m_mutex.lock();
    if (m_size <= 0) {
      m_cond.broadcast();
      m_mutex.unlock();
      return false;
    }
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    m_size++;
    m_cond.broadcast();
    m_mutex.unlock();
    return true;
  }
  bool pop(T& item) {
    m_mutex.lock();
    while (m_size <= 0) {
      if (!m_cond.wait(m_mutex.get())) {
        m_mutex.unlock();
        return false;
      }
    }
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
  }
  bool pop(T& item, int timeout_ms) {
    m_mutex.lock();
    if (m_size <= 0) {
      struct timespec t = {0, 0};
      struct timeval now = {0, 0};
      gettimeofday(&now, NULL);
      t.tv_sec = now.tv_sec + (timeout_ms / 1000);
      t.tv_nsec = (timeout_ms % 1000) * 1000;
      if (!m_cond.timewait(m_mutex.get(), t)) {
        m_mutex.unlock();
        return false;
      }
    }
    if (m_size <= 0) {
      m_mutex.unlock();
      return false;
    }
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
  }
```

以上`bool(*push)(const T& item)` 属于生产者方法，向方法中增加item。重点是成功或失败，必须广播一次，让消费者做出一次反应(有消费者的话)，而不会出现一直等待的问题。

`bool(*pop)(T& item)` 和`bool(*pop)(T& item, int timeout_ms)` 都属于消费者方法，在方法中获取可用的下一个元素。不同于直接pop元素，方法中都执行了条件锁的wait 和timedwait方法。没有超时参数`timeout_ms` 的pop方法，通过while 循环(m_size <= 0)，有元素才继续往下返回数据；有`timeout_ms` 并且(m_size <= 0) 时，则直接进入等待并设置超时，继续往下时需要再次判别元素个数(可能[1]m_cond.broadcast; 可能[2]超时了，可能还是没有元素，返回错误).

有兴趣可以查看单元测试提供的单锁示例: [unite_test/log/block_queu.cpp](../unite_test/log/block_queu.cpp)

> 特别注意`int(*pthread_cond_timedwait)(pthread_cond_t* cv, pthread_mutex_t* external_mutex, const struct timespec* t)` 方法(其他wait 方法同理)执行时会对external_mutex 先执行解锁操作，返回时(成功或超时)再次进行上锁。