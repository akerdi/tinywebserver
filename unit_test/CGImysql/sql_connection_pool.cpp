#include <gtest/gtest.h>

#include "../../src/CGImysql/sql_connection_pool.h"

static connection_pool* connPool;

static bool sql_connection_poolTest() {
  connPool = connection_pool::GetInstance();
  connPool->init("localhost", "root", "akerdi123456", "yourdb", 3306, 8, 0);
  {
    MYSQL* mysql;
    connectionRAII con(&mysql, connPool);
  }
  connPool->DestroyPool();
  return true;
}

TEST(sql_connection_poolTest, Basic) {
  EXPECT_EQ(sql_connection_poolTest(), 1);
}
