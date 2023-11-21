
#include "lst_timer.h"

sort_timer_lst::sort_timer_lst() {
  head = NULL;
  tail = NULL;
}
sort_timer_lst::~sort_timer_lst() {
  timer_item* tmp = head;
  while (tmp) {
    head = tmp->next;
    delete tmp;
    tmp = head;
  }
}

void sort_timer_lst::add_timer(timer_item* timer) {
  if (!timer)
    return;
  if (!head) {
    head = tail = timer;
    return;
  }
  if (timer->expire < head->expire) {
    head->prev = timer;
    timer->next = head;
    head = timer;
    return;
  }
  add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(timer_item* timer) {
  if (!timer)
    return;
  timer_item* tmp = timer->next;
  if (!tmp || (timer->expire < tmp->expire)) {
    return;
  }
  if (timer == head) {
    timer->next = NULL;
    tmp->prev = NULL;
    head = tmp;
    add_timer(timer, head);
  } else {
    timer->next->prev = timer->prev;
    timer->prev->next = timer->next;
    add_timer(timer, timer->next);
  }
}

void sort_timer_lst::del_timer(timer_item* timer) {
  if (!timer)
    return;
  if (timer == head && timer == tail) {
    delete timer;
    head = tail = NULL;
    return;
  }
  if (timer == head) {
    head = timer->next;
    head->prev = NULL;
    delete timer;
    return;
  }
  if (timer == tail) {
    tail = timer->prev;
    tail->next = NULL;
    delete timer;
    return;
  }
  timer->next->prev = timer->prev;
  timer->prev->next = timer->next;
  delete timer;
}
void sort_timer_lst::tick() {
  if (!head)
    return;
  time_t t = time(NULL);
  timer_item* tmp = head;
  while (tmp) {
    if (tmp->expire > t) break;

    tmp->cb_func(tmp->user_data);
    head = tmp->next;
    if (head) {
      head->prev = NULL;
    }
    delete tmp;
    tmp = head;
  }
}

void sort_timer_lst::add_timer(timer_item* timer, timer_item* lst_head) {
  timer_item* tmp = lst_head->next;

  while (tmp) {
    if (timer->expire < tmp->expire) {
      timer_item* prev = tmp->prev;
      prev->next = timer;
      tmp->prev = timer;
      timer->prev = prev;
      timer->next = tmp;
      break;
    }
    tmp = tmp->next;
  }
  if (!tmp) {
    lst_head->next = timer;
    timer->prev = lst_head;
    timer->next = NULL;
    tail = timer;
  }
}