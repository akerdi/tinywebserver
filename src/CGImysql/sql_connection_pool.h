#pragma once

#include <mysql/mysql.h>
#include <list>
#include <string>

#include "../lock/lock.h"

using namespace std;

class connection_pool {
public:
  MYSQL* GetConnection();
  bool ReleaseConnection(MYSQL* mysql);
  void DestroyPool();

  static connection_pool* GetInstance();
  void init(string url, string user, string password, string databaseName, int port, int maxConn, int close_log);

private:
  connection_pool();
  ~connection_pool();

private:
  locker m_mutex;
  list<MYSQL*> mysqlList;
  sem m_sem;
};

class connectionRAII {
public:
  connectionRAII(MYSQL** mysqlpp, connection_pool* connPool);
  ~connectionRAII();

private:
  MYSQL* mysqlRAII;
  connection_pool* connRAII;
};
