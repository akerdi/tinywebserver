#include <gtest/gtest.h>

#include "../../src/webserver.hpp"

// 演示如何使用WebServer 模板类。通过等待13s(期间有两次timeout)
// 最终第13s时发送SIGTERM 停止loop，并关闭web程序

class http_conn_test_class {
public:
  void initmysql_result(connection_pool* connPool) {}
  void init(int sockfd, const sockaddr_in& addr, char* root,
            string user, string passwd, string databaseName, int close_log)
  {

  }
  bool read() {}
  bool write() {}
  void process() {}

public:
  MYSQL* mysql;
};

static void* delayBlock(void* arg) {
  sleep(13);
  raise(SIGTERM);
}

static int test() {
  pthread_t tid;
  pthread_create(&tid, NULL, delayBlock, NULL);
  pthread_detach(tid);
  WebServer<http_conn_test_class> server;
  server.init(9006, "root", "akerdi123456", "yourdb", 1, 8, 1, 0);
  server.log_write();
  server.sql_pool();
  server.thread_pool();
  server.eventListen();
  server.eventLoop();
  printf("end server");
  return 1;
}

TEST(webserverTest, Basic) {
  EXPECT_EQ(test(), 1);
}
