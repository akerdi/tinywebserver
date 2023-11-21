# lst_timer

时间管理器，处理时间的加入/修改/删除/超时检测，使用链式存储方案。

## 变量

```cpp
class timer_item;
// 用户数据结构
struct client_data {
  sockaddr_in address;  // 连接用户地址
  int sockfd;           // 连接用户句柄
  timer_item* timer;    // 所属时间对象指针
};
// 时间对象类
class timer_item {
public:
  timer_item():prev(NULL), next(NULL) {}

public:
  time_t expire;        // 超时时间
  // 超时时调用该方法关闭socket连接(并清除epoll中的socket)
  void(*cb_func)(client_data*);
  // 保存用户数据结构
  client_data* user_data;
  timer_item* prev;     // 上一个时间对象
  timer_item* next;     // 下一个时间对象
};
// 时间对象链表管理类
class sort_timer_lst {
public:
  sort_timer_lst();
  ~sort_timer_lst();

  // 时间对象的增/改/删操作，以及超时检测方法
  void add_timer(timer_item* timer);
  void adjust_timer(timer_item* timer);
  void del_timer(timer_item* timer);
  void tick();

private:
  void add_timer(timer_item* timer, timer_item* lst_head);

  timer_item* head;     // 链表头
  timer_item* tail;     // 链表尾
};
```

由于sort_timer_lst 就是链表结构: `head(prev) <-> next0(prev) <-> next1(prev) <-> (next)tail`.

## 核心方法

```cpp
sort_timer_lst::~sort_timer_lst() {
  sort_timer_lst* tmp = head;
  while (tmp) {
    head = tmp->next;
    delete tmp;
    tmp = head;
  }
}
/**
 * + !timer，则结束
 * + !head，说明head 和tail 都为空
 * + timer->expire < head->expire，则timer改为最新head
 * + 余下的从head 往下查询插入
*/
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
/**
 * + timer 为空，放弃修改
 * + 如果没有下一个 或者timer过期时间比下一节点小，放弃修改
 * + 如果timer 为head
 *  - 则head为下一节点，并且从头添加timer
 *  - 否则提出timer后，以timer->next 开始添加timer
*/
void sort_timer_lst::adjust_timer(timer_item* timer) {
  if (!timer)
    return;
  timer_item* tmp = timer.next;
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
    add_timer(timer, tmp);
  }
}
/**
 * + timer 为空，放弃修改
 * + timer == head && timer == tail时，直接删除timer，并且重置head == tail = NULL;
 * + timer == head(说明timer != tail，因为前面判别过了)
 * + timer == tail(说明timer != head, 因为前面判别过了)
 * + timer 为中间节点，抽出来后便可删除timer
*/
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
  timer->prev->next = timer->next;
  timer->next->prev = timer->prev;
  delete timer;
}
/**
 * 定义一个tmp = head
 * 轮询 tmp
 *  + tmp->expire 大于当前时间时 break
 *  + 否则
 *    - 执行cb_func
 *    - 重置head = tmp->next
 *    - 删除tmp
 *    - tmp 置为 新的head -> 再次循环
*/
void sort_timer_lst::tick() {
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
/**
 * # 由于lst_head 是确定存在的(否则之前的函数会直接处理掉)，那么只要考虑timer 和lst_head 的下一位进行比较
 * + 设置 tmp = lst_head->next
 * + 循环 tmp
 *  - 当timer->expire < tmp->expire
 *    - 插入timer 到当前tmp前，并结束
 *    - 否则 tmp = tmp->next
 * + 再次判别如果没有tmp(可能lst_head 没有下一个)
 *  - 则timer 插到最后，成为tail
*/
void sort_timer_lst::add_timer(timer_item* timer, timer_item* lst_head) {
  timer_item* tmp = lst_head->next;
  while (tmp) {
    if (timer->expire < tmp->expire) {
      timer_item* prev = tmp->prev;
      prev->next = timer;
      tmp->pre = timer;
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
```

链式存储时间元素，是最基础简单方式方式，其他如时间轮存储，可以[查看文章](https://segmentfault.com/a/1190000044275392)。