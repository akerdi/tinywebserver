#pragma once

#include "lst_timer.h"

class Utils {
public:
  Utils() {}
  ~Utils() {}

  void init(int timeslot);
  int setnonblocking(int fd);
  void addfd(int epollfd, int sockfd, bool one_shot, int TRIGMode);

  static void sig_handler(int sig);
  void addsig(int sig, void(*handler)(int), bool restart=true);
  void timer_handler();
  void show_error(int connfd, const char* info);

public:
  int m_TIMESLOT;
  sort_timer_lst m_timer_lst;
  static int* u_pipefd;
  static int u_epollfd;
  static int u_user_count;
};

void cb_func(client_data* user_data);
