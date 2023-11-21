#include "sql_connection_pool.h"
#include "../log/log.h"

connection_pool* connection_pool::GetInstance() {
  static connection_pool instance;
  return &instance;
}
connection_pool::connection_pool() {}
connection_pool::~connection_pool() {
  DestroyPool();
}

void connection_pool::init(string url, string user, string password, string databaseName, int port, int maxConn, int close_log) {
  int m_close_log = close_log;

  MYSQL* mysql = NULL;
  for (int i = 0; i < maxConn; i++) {
    mysql = mysql_init(mysql);
    if (!mysql) {
      LOG_ERROR("Mysql init errno:%d", errno);
      exit(1);
    }
    if (!mysql_real_connect(mysql, url.c_str(), user.c_str(), password.c_str(), databaseName.c_str(), port, NULL, NULL)) {
      LOG_ERROR("Mysql real connect err:%s", mysql_error(mysql));
      exit(1);
    }
    mysqlList.push_back(mysql);
  }
  m_sem = sem(maxConn);
}
MYSQL* connection_pool::GetConnection() {
  if (0 == mysqlList.size())
		return NULL;

  m_sem.wait();

  MYSQL* mysql;

  m_mutex.lock();
  mysql = mysqlList.front();
  mysqlList.pop_front();
  m_mutex.unlock();

  return mysql;
}
bool connection_pool::ReleaseConnection(MYSQL* mysql) {
  if (!mysql)
    return false;

  m_mutex.lock();
  mysqlList.push_back(mysql);
  m_mutex.unlock();

  m_sem.post();
  return true;
}

void connection_pool::DestroyPool() {
  m_mutex.lock();
  if (mysqlList.size() > 0) {
    list<MYSQL*>::iterator it;
    for (it = mysqlList.begin(); it != mysqlList.end(); it++) {
      MYSQL* mysql = *it;
      mysql_close(mysql);
    }
    mysqlList.clear();
  }
  m_mutex.unlock();
}

connectionRAII::connectionRAII(MYSQL** mysqlpp, connection_pool* connPool) {
  *mysqlpp = connPool->GetConnection();

  mysqlRAII = *mysqlpp;
  connRAII = connPool;
}

connectionRAII::~connectionRAII() {
  connRAII->ReleaseConnection(mysqlRAII);
}
