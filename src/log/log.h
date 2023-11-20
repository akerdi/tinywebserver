#pragma once

#include <stdarg.h>
#include <string>

#include "block_queue.h"

using namespace std;

class Log {
public:
  bool init(const char* file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
  void write_log(int level, const char* format, ...);
  void flush(void);

  static Log* get_instance();
  static void* flush_log_thread(void* args);

private:
  Log();
  virtual ~Log();
  void* async_write_log();

private:
  int m_close_log;
  char dir_name[128];
  char log_name[128];
  long long m_count;
  int m_today;
  FILE* m_fp;
  char* m_buf;
  int m_log_buf_size;
  bool m_is_async;

  int m_split_lines;
  block_queue<string>* m_log_queue;
  locker m_mutex;
};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}
