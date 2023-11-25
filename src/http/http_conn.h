#pragma once

#include <string>
#include <netinet/ip.h>
#include <sys/stat.h>

#include "../CGImysql/sql_connection_pool.h"

using namespace std;

class http_conn {
public:
  static const int FILENAME_LEN = 200;
  static const int READ_BUFFER_SIZE = 2048;
  static const int WRITE_BUFFER_SIZE = 1024;
  enum METHOD {
    GET = 0,
    POST
  };
  enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
  };
  enum HTTP_CODE {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDENT_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
  };
  enum LINE_STATUS {
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
  };

public:
  http_conn() {}
  ~http_conn() {}

public:
  void init(int sockfd, const sockaddr_in& addr, char* root,
            string user, string passwd, string databaseName, int close_log);
  void close_conn(bool real_close = true);
  void process();
  bool read();
  bool write();
  sockaddr_in* get_address() {
    return &m_address;
  }
  void initmysql_result(connection_pool* connPool);

private:
  void init();
  HTTP_CODE process_read();
  LINE_STATUS parse_line();
  char* request_line();
  HTTP_CODE parse_request_line(char* text);
  HTTP_CODE parse_headers(char* text);
  HTTP_CODE parse_content(char* text);
  HTTP_CODE do_request();
  void unmap();
  bool add_response(const char* format, ...);
  bool add_content(const char* content);
  bool add_status_line(int status, const char* title);
  bool add_headers(int content_length);
  bool add_content_type();
  bool add_content_length(int content_length);
  bool add_linger();
  bool add_blank_line();
  bool process_write(HTTP_CODE code);

public:
  MYSQL* mysql;

private:
  int m_sockfd;
  sockaddr_in m_address;
  int m_close_log;
  char* doc_root;
  char sql_user[100], sql_passwd[100], sql_databaseName[100];

  char m_read_buf[READ_BUFFER_SIZE];
  long m_read_idx, m_checked_idx, m_check_line_idx;
  char m_write_buf[WRITE_BUFFER_SIZE];
  int m_write_idx;
  CHECK_STATE m_check_state;
  METHOD m_method;
  int cgi;
  char* m_string;
  char *m_url, *m_version, *m_host;
  long m_content_length;
  bool m_linger;

  char m_real_file[FILENAME_LEN];
  char* m_file_address;
  struct stat m_file_stat;
  struct iovec m_iv[2];
  int m_iv_count;
  int bytes_to_send;
  int bytes_have_send;
};
