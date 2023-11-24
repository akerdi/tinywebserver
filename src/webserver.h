#pragma once

#include <netinet/ip.h>
#include <sys/epoll.h>

#include "threadpool/threadpool.h"
#include "timer/utils.h"

const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;

template <class T>
class WebServer {
public:
  WebServer();
  ~WebServer();

  void init(int port, string user, string passWord, string databaseName,
            int opt_linger, int sql_threads_num,
            int log_write, int close_log);

  void log_write();
  void sql_pool();
  void thread_pool();
  void eventListen();
  void eventLoop();
  void timer(int sockfd, struct sockaddr_in client_address);
  void adjust_timer(timer_item* timer);
  void deal_timer(timer_item* timer, int sockfd);
  bool dealclientdata();
  bool dealwithsignal(bool& timeout, bool& stop_server);
  void dealwithread(int sockfd);
  void dealwithwrite(int sockfd);

private:
  int m_port;
  char* m_root;
  int m_log_write, m_close_log;

  int m_listenfd, m_OPT_LINGER;
  int m_pipefd[2], m_epollfd;
  T* users;
  client_data* users_timer;
  epoll_event events[MAX_EVENT_NUMBER];

  connection_pool* m_connPool;
  threadpool<T>* m_pool;
  string m_user, m_passWord, m_databaseName;
  int m_sql_threads_num;

  Utils utils;
};
