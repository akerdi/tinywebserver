# sql_connection_pool

数据库连接池的实现，靠信号量来确保正确的获取链接的sql对象。数据库连接池方便多线程请求数据时，集中式管理各个数据库连接对象，来保证业务存取的速度和质量。

## 成员变量 / 成员方法

```cpp
class connection_pool {
public:
  // 获取MYSQL* 连接对象
  MYSQL* GetConnection();
  // 归还MYSQL* 连接对象置mysqlList中
  bool ReleaseConnection(MYSQL* mysql);
  // 释放并删除所有连接池MYSQL*
  void DestroyPool();

  // 获取单例类
  static connection_pool* GetInstance();
  // 初始化时进行init所有数据库对象放入mysqlList中
  void init(string url, string user, string password, string databaseName, int port, int maxConn, int close_log);

private:
  locker m_mutex;         // 锁
  list<MYSQL*> mysqlList; // 保存MYSQL* 容器
  sem m_sem;              // 信号量用作阻塞获取MYSQL*
};
```

在`init` 时初始化多个数据库连接对象，放入mysqlList中，并初始化信号量m_sem 为对应个数的数据库连接对象。获取时使用`MYSQL*(*GetConnection)()`方法，归还时使用`bool(*ReleaseConnection)(MYSQL* mysql)`.

## 核心方法

```cpp
void connection_pool::init(string url, string user, string password, string databaseName, int port, int maxConn, int close_log) {
  MYSQL* con = NULL;
  for (int i = 0; i < maxConn; i++) {
    if (!mysql_init(con)) {
      exit(1);
    }
    if (mysql_real_connect(con, url, user, password, databaseName, port, 0, NULL) == NULL) {
      printf("Mysql real connect err:%s\n", mysql_error(con));
      exit(1);
    }
    mysqlList.push_back(con);
  }
  // 信号量重新加载为对应连接个数
  m_sem = sem(maxConn);
}
MYSQL* connection_pool::GetConnection() {
  if (0 == mysqlList.size())
    return NULL;
  // 信号量消费，等待
  m_sem.wait();

  MYSQL* mysql;

  m_mutex.lock();
  mysql = mysqlList.front();
  mysqlList.pop_front();
  m_mutex.lock();

  return mysql;
}
bool connection_pool::ReleaseConnection(MYSQL* mysql) {
  if (!mysql)
    return false;

  m_mutex.lock();
  mysqlList.push_back(mysql);
  m_mutex.unlock();
  // 信号量生产，发送信号
  m_sem.post();
  return true;
}
void connection_pool::DestroyPool() {
  m_mutex.lock();
  if (mysqlList.size() > 0) {
    List<MYSQL*>::iterator it;
    for (it = mysqlList.begin(); it != mysqlList.end(); it++) {
      MYSQL* mysql = *it;
      mysql_close(mysql);
    }
    mysqlList.clear();
  }
  m_mutex.unlock();
}
```

## RAII

```cpp
class connectionRAII {
public:
  connectionRAII(MYSQL** mysqlpp, connection_pool* connPool) {
    *mysqlpp = connPool->GetConnection();

    mysqlRAII = *mysqlpp;
    connRAII = connPool;
  }
  ~connectionRAII() {
    connRAII->ReleaseConnection(mysqlRAII);
  }

private:
  MYSQL* mysqlRAII;
  connection_pool* connRAII;
}
```

在业务层中，使用connectionRAII 来获取数据库链接对象并由系统自动释放。

> RAII 全称`资源获取即初始化`(Resource Acquisition Is Initialization), 通过生成容器函数来初始化对象，栈释放时即释放相关资源。方便正确的析构，不会出现资源泄露问题。

> 为什么connectionRAII 入参是`MYSQL**`? 因为需要对传入的空间操作，则需要传入指针。而当外部本身是指针时，需要修改指针的空间，则需要传入指针的指针，否则只是修改指针的值，其空间还是没被修改。

