#pragma once

#include <netinet/in.h>

#include "../log/log.h"

class timer_item;

struct client_data {
  sockaddr_in address;
  int sockfd;
  timer_item* timer;
};

class timer_item {
public:
  timer_item():prev(NULL), next(NULL) {}

public:
  time_t expire;

  void(*cb_func)(client_data*);
  client_data* user_data;
  timer_item* prev;
  timer_item* next;
};

class sort_timer_lst {
public:
  sort_timer_lst();
  ~sort_timer_lst();

  void add_timer(timer_item* timer);
  void adjust_timer(timer_item* timer);
  void del_timer(timer_item* timer);
  void tick();

private:
  void add_timer(timer_item* timer, timer_item* lst_head);

  timer_item* head;
  timer_item* tail;
};
