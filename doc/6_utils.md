# Utils

Utils 是作为帮助类完成一些常用的方法，如设置socket 非阻塞、给epoll 增加socket、信号的操作以及一些其他通用操作。

## 成员变量 / 成员方法

```cpp
class Utils {
public:
  Utils() {}
  ~Utils() {}

  void init(int timeslot);
  // 设置句柄为非阻塞
  int setnonblocking(int fd);
  // epoll设置监听socket
  void addfd(int epollfd, int sockfd, bool one_shot, int TRIGMode);
  // 信号(SIGTERM|SIGALRM)接收方法
  static void sig_handler(int sig);
  // 为信号重新设置方法
  void addsig(int sig, void(*handler)(int), bool restart=true);
  // SIGALRM 启动超时检测方法
  void timer_handler();
  // 向connfd发送内容，并关闭connfd
  void show_error(int connfd, const char* info);

public:
  int m_TIMESLOT;             // 时间间隔变量
  sort_timer_lst m_timer_lst; // 时间管理方法
  static int* u_pipefd;       // 全局保存
  static int u_epollfd;       // 全局保存epoll句柄
  static int u_user_count;    // 当前总连接人数
}

void Utils::sig_handler(int sig) {
  // 保存信号发生前的errno
  int save_errno = errno;
  int msg = sig;
  // send 长度设为1，因为接收时只想知道char类型长数据
  send(sig, (char*)&msg, 1, 0);
}
void Utils::addsig(int sig, void(*handler)(int), bool restart) {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = handler;
  if (restart)
    sa.sa_flags |= SA_RESTART;
  // sa_mask 如果将某个信号加入sa_mask信号集，那么在触发该信号并且执行回调函数之后期间，如果再有这个信号触发，将被阻塞，即不能同时执行两个相同的信号处理函数
  // https://blog.csdn.net/Yetao1996/article/details/124895481
  sigfillset(&sa.sa_mask);
  assert(sigaction(sig, &sa, NULL) != -1);
}
void Utils::timer_handler() {
  // 执行超时检测动作
  m_timer_lst.tick();
  // 检测完，再次发送alarm 信号
  alarm(m_TIMESLOT);
}
```

## 测试

暂无
