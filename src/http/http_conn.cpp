#include <map>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/mman.h>

#include "http_conn.h"
#include "../log/log.h"
#include "../timer/utils.h"

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;

void http_conn::initmysql_result(connection_pool* connPool) {
  MYSQL* mysql = NULL;
  connectionRAII con(&mysql, connPool);
  if (mysql_query(mysql, "SELECT username,passwd FROM user"))
  {
      LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
  }

  //从表中检索完整的结果集
  MYSQL_RES *result = mysql_store_result(mysql);

  //返回结果集中的列数
  int num_fields = mysql_num_fields(result);

  //返回所有字段结构的数组
  MYSQL_FIELD *fields = mysql_fetch_fields(result);

  //从结果集中获取下一行，将对应的用户名和密码，存入map中
  while (MYSQL_ROW row = mysql_fetch_row(result))
  {
      string temp1(row[0]);
      string temp2(row[1]);
      users[temp1] = temp2;
  }
}

int setnonbloking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}
int addfd(int epollfd, int sockfd, bool one_shot) {
  epoll_event event;
  event.data.fd = sockfd;
  event.events = EPOLLIN | EPOLLRDHUP;

  if (one_shot) event.events |= EPOLLONESHOT;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);
  setnonbloking(sockfd);
}
int modfd(int epollfd, int sockfd, int ev) {
  epoll_event event;
  event.data.fd = sockfd;
  event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
  epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &event);
}
int removefd(int epollfd, int sockfd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
  close(sockfd);
}
void http_conn::close_conn(bool real_close) {
  if (real_close && m_sockfd != -1) {
    removefd(Utils::u_epollfd, m_sockfd);
    m_sockfd = -1;
  }
}

void http_conn::init(int sockfd, const sockaddr_in& addr, char* root,
                     string user, string passwd, string databaseName, int close_log)
{
  addfd(Utils::u_epollfd, sockfd, true);
  Utils::u_user_count++;

  m_sockfd = sockfd;
  m_address = addr;
  doc_root = root;
  m_close_log = close_log;
  strcpy(sql_user, user.c_str());
  strcpy(sql_passwd, passwd.c_str());
  strcpy(sql_databaseName, databaseName.c_str());

  init();
}
void http_conn::init() {
  memset(m_read_buf, '\0', READ_BUFFER_SIZE);
  m_read_idx = 0;
  m_checked_idx = 0;
  m_check_line_idx = 0;
  memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
  m_write_idx = 0;

  m_check_state = CHECK_STATE_REQUESTLINE;
  m_method = GET;

  memset(m_real_file, '\0', FILENAME_LEN);
  m_url = m_version = m_host = NULL;
  m_content_length = 0;
  m_linger = false;

  m_file_address = NULL;
  m_iv_count = 0;

  int cgi = 0;
  m_string = NULL;
  bytes_to_send = 0;
  bytes_have_send = 0;
}

bool http_conn::read() {
  if (m_read_idx >= READ_BUFFER_SIZE)
    return false;
  int n = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return true;
    }
    return false;
  }
  m_read_idx += n;
  return true;
}
void http_conn::unmap() {
  if (m_file_address) {
    munmap(m_file_address, m_file_stat.st_size);
    m_file_address = NULL;
  }
}
bool http_conn::write() {
  if (m_write_idx >= WRITE_BUFFER_SIZE)
    return false;
  int n = -1;
  while (true) {
    n = writev(m_sockfd, m_iv, m_iv_count);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        modfd(Utils::u_epollfd, m_sockfd, EPOLLOUT);
        return true;
      }
      return false;
    }

    bytes_to_send -= n;
    bytes_have_send += n;
    if (bytes_have_send >= m_write_idx) {
      m_iv[0].iov_len = 0;
      m_iv[1].iov_base = m_file_address + bytes_have_send - m_write_idx;
      m_iv[1].iov_len = bytes_to_send;
    } else {
      m_iv[0].iov_base = m_write_buf + bytes_have_send;
      m_iv[0].iov_len = m_write_idx - bytes_have_send;
    }

    if (bytes_to_send <= 0) {
      modfd(Utils::u_epollfd, m_sockfd, EPOLLIN);
      unmap();
      if (true == m_linger) {
        init();
        return true;
      } else {
        return false;
      }
    }
  }
}
http_conn::LINE_STATUS http_conn::parse_line() {
  char tmp = NULL;
  for (; m_checked_idx < m_read_idx; m_checked_idx++) {
    tmp = m_read_buf[m_checked_idx];
    if ('\r' == tmp) {
      if ((m_checked_idx + 1) == m_read_idx)
        return LINE_OPEN;
      else if (m_read_buf[m_checked_idx+1] == '\n') {
        m_read_buf[m_checked_idx] = '\0';
        m_checked_idx++;
        m_read_buf[m_checked_idx] = '\0';
        m_checked_idx++;
        return LINE_OK;
      } else
        return LINE_BAD;
    } else if ('\n' == tmp) {
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
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
  m_url = strpbrk(text, " \t");
  if (!m_url) {
    return BAD_REQUEST;
  }
  *m_url++ = '\0';
  char *method = text;
  if (strcasecmp(method, "GET") == 0)
    m_method = GET;
  else if (strcasecmp(method, "POST") == 0) {
    m_method = POST;
    cgi = 1;
  } else
    return BAD_REQUEST;
  m_url += strspn(m_url, " \t");
  m_version = strpbrk(m_url, " \t");
  if (!m_version)
    return BAD_REQUEST;
  *m_version++ = '\0';
  m_version += strspn(m_version, " \t");
  if (strcasecmp(m_version, "HTTP/1.1") != 0)
    return BAD_REQUEST;
  if (strncasecmp(m_url, "http://", 7) == 0) {
    m_url += 7;
    m_url = strchr(m_url, '/');
  }

  if (strncasecmp(m_url, "https://", 8) == 0) {
    m_url += 8;
    m_url = strchr(m_url, '/');
  }

  if (!m_url || m_url[0] != '/')
    return BAD_REQUEST;
  //当url为/时，显示判断界面
  if (strlen(m_url) == 1)
    strcat(m_url, "judge.html");
  m_check_state = CHECK_STATE_HEADER;
  return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
  if ('\0' == text[0]) {
    if (m_content_length > 0) {
      m_check_state = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
    return GET_REQUEST;
  } else if (strncasecmp(text, "Connection:", 11) == 0) {
    text += 11;
    text += strspn(text, " \t");
    if (strcasecmp(text, "keep-alive") == 0) {
      m_linger = true;
    }
  } else if (strncasecmp(text, "Content-length:", 15) == 0) {
    text += 15;
    text += strspn(text, " \t");
    m_content_length = atol(text);
  } else if (strncasecmp(text, "Host:", 5) == 0) {
    text += 5;
    text += strspn(text, " \t");
    m_host = text;
  } else {
    LOG_INFO("oop!unknow header: %s", text);
  }
  return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
  if (m_read_idx >= m_content_length + m_checked_idx) {
    text[m_content_length] = '\0';
    m_string = text;
    return GET_REQUEST;
  }
  return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::do_request() {
  strcpy(m_real_file, doc_root);
  int len = strlen(doc_root);
  //printf("m_url:%s\n", m_url);
  const char *p = strrchr(m_url, '/');

  //处理cgi
  if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
    //根据标志判断是登录检测还是注册检测
    char flag = m_url[1];

    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/");
    strcat(m_url_real, m_url + 2);
    strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
    free(m_url_real);

    //将用户名和密码提取出来
    //user=123&passwd=123
    char name[100], password[100];
    int i;
    for (i = 5; m_string[i] != '&'; ++i)
      name[i - 5] = m_string[i];
    name[i - 5] = '\0';

    int j = 0;
    for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
      password[j] = m_string[i];
    password[j] = '\0';

    if (*(p + 1) == '3') {
      //如果是注册，先检测数据库中是否有重名的
      //没有重名的，进行增加数据
      char *sql_insert = (char *)malloc(sizeof(char) * 200);
      strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
      strcat(sql_insert, "'");
      strcat(sql_insert, name);
      strcat(sql_insert, "', '");
      strcat(sql_insert, password);
      strcat(sql_insert, "')");

      if (users.find(name) == users.end()) {
        m_lock.lock();
        int res = mysql_query(mysql, sql_insert);
        users.insert(pair<string, string>(name, password));
        m_lock.unlock();

        if (!res)
          strcpy(m_url, "/log.html");
        else
          strcpy(m_url, "/registerError.html");
      }
      else
        strcpy(m_url, "/registerError.html");
    }
    //如果是登录，直接判断
    //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
    else if (*(p + 1) == '2') {
      if (users.find(name) != users.end() && users[name] == password)
        strcpy(m_url, "/welcome.html");
      else
        strcpy(m_url, "/logError.html");
    }
  }

  if (*(p + 1) == '0') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/register.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else if (*(p + 1) == '1') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/log.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else if (*(p + 1) == '5') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/picture.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else if (*(p + 1) == '6') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/video.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else if (*(p + 1) == '7') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/fans.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

  if (stat(m_real_file, &m_file_stat) != 0) {
    return NO_RESOURCE;
  }
  if (!(m_file_stat.st_mode & S_IROTH)) {
    return FORBIDDENT_REQUEST;
  }
  if (S_ISDIR(m_file_stat.st_mode)) {
    return BAD_REQUEST;
  }

  int fd = open(m_real_file, O_RDONLY);
  m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  return FILE_REQUEST;
}
http_conn::HTTP_CODE http_conn::process_read() {
  LINE_STATUS linestatus = LINE_OPEN;
  char* text = NULL;
  HTTP_CODE ret = NO_REQUEST;
  while ((m_check_state == CHECK_STATE_CONTENT && ret == NO_REQUEST) || ((linestatus = parse_line()) == LINE_OK)) {
    text = request_line();
    m_check_line_idx = m_checked_idx;
    LOG_INFO("text: %s", text);
    switch (m_check_state) {
      case CHECK_STATE_REQUESTLINE: {
        ret = parse_request_line(text);
        if (ret == BAD_REQUEST)
          return BAD_REQUEST;
      }
        break;
      case CHECK_STATE_HEADER: {
        ret = parse_headers(text);
        if (ret == GET_REQUEST)
          return do_request();
        else if (ret == BAD_REQUEST)
          return BAD_REQUEST;
      }
        break;
      case CHECK_STATE_CONTENT: {
        ret = parse_content(text);
        if (ret == GET_REQUEST)
          return do_request();
      }
        break;
      default: return INTERNAL_ERROR;
    }
  }
  return ret;
}
bool http_conn::add_response(const char* format, ...) {
  if (m_write_idx >= WRITE_BUFFER_SIZE) {
    return false;
  }
  va_list va;
  va_start(va, format);

  int n = vsnprintf(m_write_buf+m_write_idx, WRITE_BUFFER_SIZE-m_write_idx-1, format, va);
  if (n >= WRITE_BUFFER_SIZE - m_write_idx-1) {
    va_end(va);
    return false;
  }
  m_write_idx += n;
  va_end(va);
  return true;
}
bool http_conn::add_content(const char* content) {
  return add_response("%s", content);
}
bool http_conn::add_status_line(int status, const char* title) {
  return add_response("HTTP/1.1 %d %s\r\n", status, title);
}
bool http_conn::add_headers(int content_length) {
  return add_content_length(content_length) && add_blank_line();
}
bool http_conn::add_content_type() {
  return add_response("Content-Type: text/html\r\n");
}
bool http_conn::add_content_length(int content_length) {
  return add_response("Content-Length: %d\r\n", content_length);
}
bool http_conn::add_linger() {
  return add_response("keep-alive: true\r\n");
}
bool http_conn::add_blank_line() {
  return add_response("\r\n");
}
bool http_conn::process_write(HTTP_CODE code) {
  switch (code) {
    case BAD_REQUEST: {
      add_status_line(400, error_400_title);
      add_headers(strlen(error_400_form));
      if (!add_content(error_400_form))
        return false;
    }
      break;
    case INTERNAL_ERROR: {
      add_status_line(500, error_500_title);
      add_headers(strlen(error_500_form));
      if (!add_content(error_500_form))
        return false;
    }
      break;
    case NO_RESOURCE: {
      add_status_line(404, error_404_title);
      add_headers(strlen(error_404_form));
      if (!add_content(error_404_form))
        return false;
    }
      break;
    case FORBIDDENT_REQUEST: {
      add_status_line(403, error_403_title);
      add_headers(strlen(error_403_form));
      if (!add_content(error_403_form))
        return false;
    }
      break;
    case FILE_REQUEST: {
      add_status_line(200, ok_200_title);
      if (m_file_stat.st_size > 0) {
        add_headers(m_file_stat.st_size);
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
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
  m_iv[0].iov_base = m_write_buf;
  m_iv[0].iov_len = m_write_idx;
  m_iv_count = 1;
  bytes_to_send = m_write_idx;
  return true;
}
char* http_conn::request_line() {
  return m_read_buf + m_check_line_idx;
}

void http_conn::process() {
  HTTP_CODE read_ret = process_read();
  if (read_ret == NO_REQUEST) {
    modfd(Utils::u_epollfd, m_sockfd, EPOLLIN);
    return;
  }
  bool write_ret = process_write(read_ret);
  if (!write_ret) {
    close_conn();
    return;
  }
  modfd(Utils::u_epollfd, m_sockfd, EPOLLOUT);
}
