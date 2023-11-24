#pragma once

#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>

#include "webserver.h"

template <class T>
WebServer<T>::WebServer() {
  char server_path[256];
  getcwd(server_path, sizeof(server_path)-1);
  char root[] = "/root";
  m_root = (char*)malloc(strlen(server_path) + strlen(root) + 1);
  strcpy(m_root, server_path);
  strcat(m_root, root);

  users = new T[MAX_FD];
  users_timer = new client_data[MAX_FD];
}
template <class T>
WebServer<T>::~WebServer() {
  close(m_listenfd);
  close(m_pipefd[0]);
  close(m_pipefd[1]);
  close(m_epollfd);
  delete [] m_root;
  delete [] users;
  delete [] users_timer;
}
template <class T>
void WebServer<T>::init(int port, string user, string passWord, string databaseName,
                        int opt_linger, int sql_threads_num,
                        int log_write, int close_log)
{
  m_port = port;
  m_user = user;
  m_passWord = passWord;
  m_databaseName = databaseName;
  m_OPT_LINGER = opt_linger;
  m_sql_threads_num = sql_threads_num;
  m_log_write = log_write;
  m_close_log = close_log;
}
template <class T>
void WebServer<T>::log_write() {
  if (0 == m_close_log) {
    if (1 == m_log_write) {
      Log::get_instance()->init("./Server.log", m_close_log, 2000, 800000, 800);
    } else {
      Log::get_instance()->init("./Server.log", m_close_log, 2000, 800000, 0);
    }
  }
}
template <class T>
void WebServer<T>::sql_pool() {
  m_connPool = connection_pool::GetInstance();
  m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_threads_num, m_close_log);
  users->initmysql_result(m_connPool);
}
template <class T>
void WebServer<T>::thread_pool() {
  m_pool = new threadpool<T>(m_connPool);
}
template <class T>
void WebServer<T>::eventListen() {
  m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
  assert(m_listenfd >= 0);

  if (0 == m_OPT_LINGER) {
    struct linger opt = {0, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &opt, sizeof(opt));
  } else {
    struct linger opt = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &opt, sizeof(opt));
  }
  int opt = 1;
  setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  int ret = -1;
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = htons(m_port);
  address.sin_addr.s_addr = htonl(INADDR_ANY);

  ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
  assert(ret >= 0);
  ret = listen(m_listenfd, 5);
  assert(ret >= 0);

  m_epollfd = epoll_create(1);
  assert(m_epollfd >= 0);
  utils.addfd(m_epollfd, m_listenfd, false);

  ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_pipefd);
  assert(ret >= 0);
  utils.addfd(m_epollfd, m_pipefd[0], false);
  utils.setnonblocking(m_pipefd[1]);

  utils.addsig(SIGPIPE, SIG_IGN);
  utils.addsig(SIGALRM, utils.sig_handler, false);
  utils.addsig(SIGTERM, utils.sig_handler, false);
  alarm(TIMESLOT);

  utils.init(TIMESLOT);
  Utils::u_pipefd = m_pipefd;
  Utils::u_epollfd = m_epollfd;
}
template <class T>
void WebServer<T>::timer(int sockfd, struct sockaddr_in client_address) {
  users[sockfd].init(sockfd, client_address, m_root, m_user, m_passWord, m_databaseName, m_close_log);

  timer_item* timer = new timer_item;
  timer->cb_func = cb_func;
  time_t t = time(NULL);
  timer->expire = t + 3 * TIMESLOT;
  timer->user_data = &users_timer[sockfd];

  users_timer[sockfd].sockfd = sockfd;
  users_timer[sockfd].address = client_address;
  users_timer[sockfd].timer = timer;

  utils.m_timer_lst.add_timer(timer);
}
template <class T>
void WebServer<T>::adjust_timer(timer_item* timer) {
  if (timer) {
    time_t t = time(NULL);
    timer->expire = t + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);
    LOG_INFO("time adjust: %d", timer->user_data->sockfd);
  }
}
template <class T>
void WebServer<T>::deal_timer(timer_item* timer, int sockfd) {
  if (timer) {
    timer->cb_func(timer->user_data);
    utils.m_timer_lst.del_timer(timer);
  }
}
template <class T>
bool WebServer<T>::dealwithsignal(bool& timeout, bool& stop_server) {
  static char msg[512];
  int ret = recv(m_pipefd[0], msg, sizeof(msg)-1, 0);
  if (ret < 0) {
    return false;
  }
  for (int i = 0 ; i < ret; i++) {
    switch (msg[i])
    {
    case SIGALRM: {
      timeout = true;
    }
      break;
    case SIGTERM: {
      stop_server = true;
    }
      break;
    }
  }
  return true;
}
template <class T>
void WebServer<T>::dealwithread(int sockfd) {
  timer_item* timer = users_timer[sockfd].timer;
  if (users[sockfd].read()) {
    adjust_timer(timer);
    m_pool->append(users + sockfd);
  } else {
    deal_timer(timer, sockfd);
  }
}
template <class T>
void WebServer<T>::dealwithwrite(int sockfd) {
  timer_item* timer = users_timer[sockfd].timer;
  if (users[sockfd].write()) {
    adjust_timer(timer);
  } else {
    deal_timer(timer, sockfd);
  }
}
template <class T>
bool WebServer<T>::dealclientdata() {
  struct sockaddr_in address;
  socklen_t len = sizeof(address);
  int sockfd = accept(m_listenfd, (struct sockaddr*)&address, &len);
  if (sockfd < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return true;
    }
    return false;
  }
  timer(sockfd, address);
  return true;
}
template <class T>
void WebServer<T>::eventLoop() {
  bool stop_server = false;
  bool timeout = false;
  while (!stop_server) {
    int n = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
    if (n < 0 && errno != EINTR) {
      break;
    }
    for (int i = 0; i < n; i++) {
      int sockfd = events->data.fd;
      if (m_listenfd == sockfd) {
        if (!dealclientdata())
          LOG_ERROR("deal client data failure!");
      } else if (events->events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        timer_item* timer = users_timer[i].timer;
        deal_timer(timer, sockfd);
      } else if (sockfd == m_pipefd[0] && events->events & EPOLLIN) {
        if (!dealwithsignal(timeout, stop_server))
          LOG_ERROR("deal signal failure!");
      } else if (events->events & EPOLLIN) {
        dealwithread(sockfd);
      } else if (events->events & EPOLLOUT) {
        dealwithwrite(sockfd);
      }
    }
    if (timeout) {
      utils.timer_handler();
      LOG_INFO("time tick");
      timeout = false;
    }
  }
}
