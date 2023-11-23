#include <gtest/gtest.h>
#include <mysql/mysql.h>

#include "../../src/threadpool/threadpool.h"
#include "../../src/lock/lock.h"

#define loop_count 20

int count = 0;
locker m_mutex;

class Http_conn_test_class {
public:
  void process() {
    m_mutex.lock();
    count++;
    m_mutex.unlock();
  }

public:
  int m_state;
  MYSQL* mysql;
};

static int test() {
  connection_pool* connPool = connection_pool::GetInstance();
  connPool->init("localhost", "root", "akerdi123456", "yourdb", 3306, 8, 0);
  Http_conn_test_class https[loop_count];
  threadpool<Http_conn_test_class>* pool = new threadpool<Http_conn_test_class>(connPool);
  int n = sizeof(https) / sizeof(Http_conn_test_class);
  for (int i = 0; i < n; i++) {
    pool->append(https+i);
  }
  return 1;
}

TEST(threadpoolTest, Basic) {
  EXPECT_EQ(test(), 1);
  sleep(1);
  EXPECT_EQ(count, loop_count);
  connection_pool::GetInstance()->DestroyPool();
}
