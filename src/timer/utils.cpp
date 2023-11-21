#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <string.h>

#include "utils.h"

void Utils::init(int timeslot) {
  m_TIMESLOT = timeslot;
}
int Utils::setnonblocking(int fd) {
  int old_opt = fcntl(fd, F_GETFL);
  int new_opt = old_opt | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_opt);
  return old_opt;
}
void Utils::addfd(int epollfd, int sockfd, bool one_shot, int TRIGMode) {
  epoll_event event;
  event.data.fd = sockfd;
  if (1 == TRIGMode) {
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
  } else {
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
  }
  if (one_shot) {
    event.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);
  setnonblocking(sockfd);
}

void Utils::sig_handler(int sig) {
  int saved_errno = errno;
  int msg = sig;
  send(sig, (char*)msg, 1, 0);
  errno = saved_errno;
}
void Utils::addsig(int sig, void(*handler)(int), bool restart=true) {
  struct sigaction sa;
  sa.sa_handler = handler;
  sigfillset(&sa.sa_mask);
  if (restart) {
    sa.sa_flags |= SA_RESTART;
  }
  assert(sigaction(sig, &sa, NULL) != 0);
}
void Utils::timer_handler() {
  m_timer_lst.tick();
  alarm(m_TIMESLOT);
}
void Utils::show_error(int connfd, const char* info) {
  send(connfd, info, strlen(info), 0);
  close(connfd);
}

int* Utils::u_pipefd = NULL;
int Utils::u_epollfd = -1;
int Utils::u_user_count = 0;

void cb_func(client_data* user_data) {
  epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, NULL);
  close(user_data->sockfd);
  Utils::u_user_count--;
}
