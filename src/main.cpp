#include "webserver.hpp"
#include "http/http_conn.h"

int main(int argc, char* argv[]) {
  WebServer<http_conn> server;

  const int port = 9006;
  const char db_user[] = "user";
  const char db_pass[] = "pass";
  const char db_name[] = "name";
  const int opt_linger = 1;
  const int threads_num = 8;
  const int is_log_write = 1;
  const int is_close_log = 0;

  server.init(port, db_user, db_pass, db_name,opt_linger, threads_num, is_log_write, is_close_log);
  server.log_write();
  server.sql_pool();
  server.thread_pool();
  server.eventListen();
  server.eventLoop();
  return 0;
}