# http_conn 业务类

WebServer 实现了socket，epoll等的创建和管理，而WebServer 是模板类，需要提供一个业务类来实现具体业务，本章就是完成业务部分的内容。

http_conn 类中会描述重要的知识，而其中作为展示业务的部分则推荐用户理解即可，如首次加载数据库方法`void http_conn::initmysql_result(connection_pool* connPool)`，如HTTP 状态码及说明文，如`HTTP_CODE http_conn::do_request()`方法等。

如果不了解HTTP协议或者没有上心记忆该规则，可以通过command `curl https://baidu.com -v` 查看完整内容。

<!-- 当前文档会展示核心方法 -->

## 头文件

```cpp
class http_conn {
  ...
public:
  // 重置信息入口
  void init(int sockfd, const sockaddr_in& addr, char* root,
            string user, string passwd, string databaseName, int close_log);
  // 关闭当前连接
  void close_conn(bool real_close = true);
  // 对外暴露的任务执行方法
  void process();
  // 从socket中读入数据
  bool read();
  // 从socket中发送数据
  bool write();
  sockaddr_in* get_address() {
    return &m_address;
  }
  // 数据库数据加载到内存
  // 本例中在内存存放数据库用户数据，实际案例中是不对的。
  // 原作者采用这种方法，考虑的是重点突出C++服务器的服务过程。
  // 有意向学习数据库实际应用可以多加查看其他应用，不必在意该实现
  void initmysql_result(connection_pool* connPool);

private:
  // 重新初始化成员属性
  void init();
  // 解析并执行用户请求
  HTTP_CODE process_read();
  // 从已读取字符串识别`\r``\n`后转为`\0``\0`
  LINE_STATUS parse_line();
  // 从已读取字符串的指针偏移处获取下一行字符串
  char* request_line();
  // 解析请求首行内容
  HTTP_CODE parse_request_line(char* text);
  // 解析请求头相关参数
  HTTP_CODE parse_headers(char* text);
  // 解析请求头内容
  HTTP_CODE parse_content(char* text);
  // 执行用户请求，业务相关
  HTTP_CODE do_request();
  // 释放mmap中的文件内存
  void unmap();
  // 拼接返回数据
  bool add_response(const char* format, ...);
  // 以下根据add_response 拼接返回的字符数据
  bool add_content(const char* content);
  bool add_status_line(int status, const char* title);
  bool add_headers(int content_length);
  bool add_content_type();
  bool add_content_length(int content_length);
  bool add_linger();
  bool add_blank_line();
  // 对用户执行结果做出反馈(如状态码相关返回信息，以及发送文件示例)
  bool process_write(HTTP_CODE code);

public:
  // 从connectionRAII中获取到MYSQL对象
  MYSQL* mysql;

private:
  // 经过public:init 方法传入
  int m_sockfd;
  sockaddr_in m_address;
  int m_close_log;
  char* doc_root;
  char sql_user[100], sql_passwd[100], sql_databaseName[100];

  // 从socket中读入的内容
  char m_read_buf[READ_BUFFER_SIZE];
  // 读入内容大小 | 解析内容字符索引 | 读取行数据索引
  long m_read_idx, m_checked_idx, m_check_line_idx;
  // 发送内容
  char m_write_buf[WRITE_BUFFER_SIZE];
  // 发送内容大小
  int m_write_idx;
  // 读取块进度 request_line -> header -> content
  CHECK_STATE m_check_state;
  // 解析结果
  METHOD m_method;
  int cgi;
  char* m_string;
  char *m_url, *m_version, *m_host;
  long m_content_length;
  bool m_linger;

  // 执行结果数据
  // 返回文件类型时的真是文件名
  char m_real_file[FILENAME_LEN];
  // mmap对应的文件指针
  char* m_file_address;
  // 返回文件类型的文件信息
  struct stat m_file_stat;
  // 返回数据的向量结构数组
  struct iovec m_iv[2];
  // 返回数据的向量数组个数
  int m_iv_count;
  // 将要返回数据大小
  int bytes_to_send;
  // 已返回数据大小
  int bytes_have_send;
};
```

<!-- > 主要是示例C++中服务器的相关知识，其中数据库部分仅仅示例 -->

## read | write | process

由于http_conn 在WebServer 构造函数时便初始化了所有的对象，保存在`users` 中，故每次使用时，只需重置为初始数据即可

```cpp
void http_conn::init(int sockfd, const sockaddr_in& addr, char* root,
                     string user, string passwd, string databaseName, int close_log)
{
  // 将sockfd 添加到epoll IO中监听EPOLLIN
  addfd(Utils::u_epollfd, sockfd, true);
  // 记录增加了新的用户连接
  Utils::u_user_count++;

  m_sockfd = sockfd;
  m_address = addr;
  doc_root = root;
  m_close_log = close_log;
  strcpy(sql_user, user.c_str());
  strcpy(sql_passwd, passwd.c_str());
  strcpy(sql_databaseName, databaseName.c_str());

  // 初始化所有变量
  init();
}
// 初始化所有非传入属性
void http_conn::init() {
  ...
}
```

接下来完成对外暴露的read / write / process 方法:

```cpp
// 读入socket 缓存中内容
bool http_conn::read() {
  // 如果读取的数据大于限制，则报错
  if (m_read_idx >= READ_BUFFER_SIZE)
    return false;
  // 读取输入数据, 由于发送一段buffer可能分多次，所以读取来源由指针偏移来计算
  int n = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return true;
    }
    return false;
  }
  m_read_idx += n;
  return true;
}
// 返回数据
bool http_conn::write() {
  // 返回数据大于限制，则报错
  if (m_write_idx >= WRITE_BUFFER_SIZE)
    return false;
  int n = -1;
  // 循环返回数据，直到全部返回成功，才结束
  while (true) {
    // ssize_t writev(int fd, const struct iovec* iov, int iovcnt)
    n = writev(m_sockfd, m_iv, m_iv_count);
    if (n < 0) {
      // 如果出错: 1. 被打断则修改为EPOLLOUT继续返回数据；2. 结束此次返回动作
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        modfd(Utils::u_epollfd, m_sockfd, EPOLLOUT);
        return true;
      }
      return false;
    }
    bytes_to_send -= n;
    bytes_have_send += n;
    // 大文件发送可能导致writev会多次发送，但是数据结构体中每次传输不糊自动偏移文件指针和传输长度，导致卡在那
    // 测试方法是注释下方的逻辑块，让数据结构体的m_iv[1].iov_base 指针不根据已发送内容偏移
    // https://zwiley.github.io/mybook/webserver/14%20%E9%A1%B9%E7%9B%AE%E9%97%AE%E9%A2%98%E6%B1%87%E6%80%BB/
    if (bytes_have_send >= m_write_idx) {
      m_iv[0].iov_len = 0;
      m_iv[1].iov_base = m_file_address + bytes_have_send - m_write_idx;
      m_iv[1].iov_len = bytes_to_send;
    } else {
      m_iv[0].iov_base = m_write_buf + bytes_have_send;
      m_iv[0].iov_len = m_write_idx - bytes_have_send;
    }

    if (bytes_to_send <= 0) {
      // 发送内容完毕后，首先将IO状态改为EPOLLIN
      modfd(Utils::u_epollfd, m_sockfd, EPOLLIN);
      unmap();
      // 如果header中有linger(keepalive)，则监听为EPOLLIN一段时间
      if (true == m_linger) {
        init();
        return true;
      } else {
        return false;
      }
    }
  }
}
// 从read之后，进入process方法
void http_conn::process() {
  // process_read 识别请求意图，并执行业务
  HTTP_CODE read_ret = process_read();
  // 有可能当前数据不完整，则继续EPOLLIN监听更多输入
  if (read_ret == NO_REQUEST) {
    modfd(Utils::u_epollfd, m_sockfd, EPOLLIN);
    return;
  }
  // 将执行业务状态码返回为返回数据内容
  bool write_ret = process_write(read_ret);
  if (!write_ret) {
    close_conn();
    return;
  }
  // IO状态从EPOLLIN 改为EPOLLOUT，开始执行返回数据(write方法)
  modfd(Utils::u_epollfd, m_sockfd, EPOLLOUT);
}
```

> 注意一点，读取和返回数据中，socket 是通过kernel 发送内容。kernel无法保证每次发送的大小。根据api返回大小，拼接后就是当前操作了的数据量。

## 解析

读取请求数据，根据状态机设计模式读取请求数据:

```cpp
// 解析根据状态机设计模式
http_conn::HTTP_CODE http_conn::process_read() {
  LINE_STATUS linestatus = LINE_OPEN;
  char* text = NULL;
  HTTP_CODE ret = NO_REQUEST;
  // 1. 如果解析状态为CHECK_STATE_CONTENT 且 没有结果，满足条件，继续解析
  // 2. 读取字符解析出一行字符串，满足条件
  while ((m_check_state == CHECK_STATE_CONTENT && ret == NO_REQUEST) || ((linestatus = parse_line()) == LINE_OK)) {
    // 根据读入字符串的指针偏移和已解析字符串\0得到一行字符串
    text = request_line();
    // 计算指针偏移用
    m_check_line_idx = m_checked_idx;
    LOG_INFO("text: %s", text);
    switch (m_check_state) {
      case CHECK_STATE_REQUESTLINE: {
        ret = parse_request_line(text);
        // 失败时退出
        if (ret == BAD_REQUEST)
          return BAD_REQUEST;
      }
        break;
      case CHECK_STATE_HEADER: {
        ret = parse_headers(text);
        // 读取到空白行，并且没有请求体，则执行业务语句
        if (ret == GET_REQUEST)
          return do_request();
        // 失败时退出
        else if (ret == BAD_REQUEST)
          return BAD_REQUEST;
      }
        break;
      case CHECK_STATE_CONTENT: {
        ret = parse_content(text);
        // 满足请求体长度，则执行业务语句
        if (ret == GET_REQUEST)
          return do_request();
      }
        break;
      default: return INTERNAL_ERROR;
    }
  }
  return ret;
}
http_conn::LINE_STATUS http_conn::parse_line() {
  char tmp = NULL;
  for (; m_checked_idx < m_read_idx; m_checked_idx++) {
    tmp = m_read_buf[m_checked_idx];
    if ('\r' == tmp) {
      // \r\n\0
      // 如果\r+1 == m_read_idx 说明缺了一个，需要继续等待接收
      // 说明字符不完整, 认定为LINE_OPEN
      if ((m_checked_idx + 1) == m_read_idx)
        return LINE_OPEN;
      else if (m_read_buf[m_checked_idx+1] == '\n') {
        // \r -> \0; 并且m_checked_idx++移到\n
        m_read_buf[m_checked_idx] = '\0';
        m_checked_idx++;
        // \n -> \0; 并且m_checked_idx++移到下一个值
        m_read_buf[m_checked_idx] = '\0';
        m_checked_idx++;
        return LINE_OK;
      } else
        return LINE_BAD;
    } else if ('\n' == tmp) {
      // 补上之前\r 缺失\n 的逻辑
      if (m_checked_idx > 0 && m_read_buf[m_checked_idx-1] == '\r') {
        m_read_buf[m_checked_idx-1] = '\0';
        m_read_buf[m_checked_idx] = '\0';
        m_checked_idx++;
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_OPEN;
}
// 识别请求头第一行的请求行
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
  ...
  // 切换解析状态为CHECK_STATE_HEADER
  m_check_state = CHECK_STATE_HEADER;
  // 返回NO_REQUEST，解析状态需要继续
  return NO_REQUEST;
}
// 解析请求头
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
  // 行字符串第一个字符为'\0'
  if ('\0' == text[0]) {
    // 如果有content_length，则说明有请求体
    if (m_content_length > 0) {
      // 切换解析状态为CHECK_STATE_CONTENT，并且解析状态继续
      m_check_state = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
    // 空白行的下一行为空，则为GET请求，解析状态完毕
    return GET_REQUEST;
  } else if
  ...
  // 解析状态继续
  return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
  // 总读取字符长度 > (已解析长度(request_line + header) + 请求体长度)
  // 此时说明满足一个完整请求，则将text赋给m_string待接下来解析
  if (m_read_idx >= m_content_length + m_checked_idx) {
    text[m_content_length] = '\0';
    m_string = text;
    return GET_REQUEST;
  }
  // 未满足完整请求，则继续解析
  return NO_REQUEST;
}
```

## 执行业务

执行业务语句是根据当前已掌握的请求数据，返回对应逻辑。学过后端都知道这块业务突出一个业务编写根据产品需求返回。一般来说，这块都是由php/python/js 等高级语言编写，突出一个快字。当前仅是为了便于学习，我们直接跳过业务部分，到达最后功能部分代码:

```cpp
http_conn::HTTP_CODE http_conn::do_request() {
  ...
  int fd = open(m_real_file, O_RDONLY);
  // void*(*mmap)(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
  m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  // 内存已经加载好了，不需要该文件句柄了
  close(fd);
  // 返回文件内容状态
  return FILE_REQUEST;
}
```

## 生成返回内容

当 `proccess_read() { return do_request(); }` 返回得到正确结果后，`process_write`需要为返回的数据进行整理:

```cpp
bool http_conn::process_write(HTTP_CODE code) {
  switch (code) {
    case ...
      break;
    case FILE_REQUEST: {
      add_status_line(200, ok_200_title);
      if (m_file_stat.st_size > 0) {
        // 如果文件获取到了大小，则为返回值增加Content-Length 内容
        add_headers(m_file_stat.st_size);
        // 为返回值向量数据结构体补充内容
        // 0中放返回数据头
        // 1中放返回数据体
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
        // 记录向量个数和总的要返回的大小
        m_iv_count = 2;
        bytes_to_send = m_write_idx + m_file_stat.st_size;
        return true;
      } else {
        const char* msg = "<html><body></body></html>";
        add_headers(strlen(msg));
        if (!add_content(msg))
          return false;
      }
    }
      break;
    default: return false;
  }
  // 除了FILE_REQUEST，其他都是返回数据头，那么只需要提供向量0的内容
  m_iv[0].iov_base = m_write_buf;
  m_iv[0].iov_len = m_write_idx;
  m_iv_count = 1;
  bytes_to_send = m_write_idx;
  return true;
}
bool http_conn::add_response(const char* format, ...) {
  // 超过返回数据限制大小，则返回错误
  if (m_write_idx >= WRITE_BUFFER_SIZE) {
    return false;
  }
  va_list va;
  va_start(va, format);

  int n = vsnprintf(m_write_buf+m_write_idx, WRITE_BUFFER_SIZE-m_write_idx-1, format, va);
  // 判断是否超过最大数据限制
  if (n >= WRITE_BUFFER_SIZE - mwrite_idx-1) {
    va_end(va);
    return false;
  }
  // 累计返回数据大小
  m_write_idx += n;
  va_end(va);
  return true;
}
```
