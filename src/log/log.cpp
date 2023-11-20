#include <string.h>

#include "log.h"

Log::Log() {
  m_count = 0;
  m_is_async = false;
}

Log::~Log() {
  if (m_fp) {
    fclose(m_fp);
  }
}

bool Log::init(const char* file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size) {
  if (max_queue_size > 0) {
    m_log_queue = new block_queue<string>(max_queue_size);
    pthread_t tid;
    pthread_create(&tid, NULL, Log::flush_log_thread, NULL);
    pthread_detach(tid);
    m_is_async = true;
  }

  m_close_log = close_log;
  m_log_buf_size = log_buf_size;
  m_buf = (char*)malloc(m_log_buf_size);
  memset(m_buf, '\0', m_log_buf_size);
  m_split_lines = split_lines;

  time_t t = time(NULL);
  struct tm* sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;

  m_today = my_tm.tm_mday;

  const char* p = strrchr(file_name, '/');
  char log_full_name[256] = {NULL};

  if (p == NULL) {
    strcpy(log_name, file_name);
    strcpy(dir_name, "");
    snprintf(log_full_name, sizeof(log_full_name)-1, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, log_name);
  } else {
    strcpy(log_name, p+1);
    strncpy(dir_name, file_name, p-file_name-1);
    snprintf(log_full_name, sizeof(log_full_name)-1, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, log_name);
  }

  m_fp = fopen(log_full_name, "a");
  if (!m_fp) return false;

  return true;
}

void Log::write_log(int level, const char* format, ...) {
  struct timeval now = {NULL, NULL};
  gettimeofday(&now, NULL);
  time_t t = now.tv_sec;
  struct tm* sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;

  char s[16] = {NULL};
  switch (level) {
    case 0: {
      strcpy(s, "[DEBUG]:");
    }
      break;
    case 1: {
      strcpy(s, "[INFO]:");
    }
      break;
    case 2: {
      strcpy(s, "[INFO]:");
    }
      break;
    case 3: {
      strcpy(s, "[INFO]:");
    }
      break;
    default: {
      sprintf(s, "[Undefine:%d]:", level);
    }
      break;
  }

  m_mutex.lock();
  m_count++;
  if (my_tm.tm_mday != m_today || m_count % m_split_lines == 0) {
    fflush(m_fp);
    fclose(m_fp);
    char new_log[256] = {NULL};
    char tail[16] = {NULL};

    snprintf(tail, sizeof(tail)-1, "%d_%02d_%02d_", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday);
    if (my_tm.tm_mday != m_today) {
      m_today = my_tm.tm_mday;
      m_count = 0;
      snprintf(new_log, sizeof(new_log)-1, "%s%s%s", dir_name, tail, log_name);
    } else {
      snprintf(new_log, sizeof(new_log)-1, "%s%s%s.%lld", dir_name, tail, log_name, m_count);
    }
    m_fp = fopen(new_log, "a");
  }
  m_mutex.unlock();

  string log_str;

  va_list va;
  va_start(va, format);
  m_mutex.lock();
  int n = snprintf(m_buf, 48, "%d-%02d-%02d %0d2:%02d:%02d.%06ld %s ",
                    my_tm.tm_year + 1900, my_tm.tm_mon+1, my_tm.tm_mday,
                    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
  int m = vsnprintf(m_buf+n, m_log_buf_size-n-1, format, va);
  m_buf[n+m] = '\n';
  m_buf[n+m+1] = '\0';
  log_str = m_buf;
  m_mutex.unlock();

  va_end(va);

  if (m_is_async && !m_log_queue->full()) {
    m_log_queue->push(log_str);
  } else {
    m_mutex.lock();
    fputs(log_str.c_str(), m_fp);
    m_mutex.unlock();
  }
}

void Log::flush(void) {
  m_mutex.lock();
  fflush(m_fp);
  m_mutex.unlock();
}

Log* Log::get_instance() {
  static Log instance;
  return &instance;
}
void* Log::flush_log_thread(void* args) {
  Log::get_instance()->async_write_log();
}
void* Log::async_write_log() {
  string single_log;
  while (m_log_queue->pop(single_log)) {
    puts(single_log.c_str());
    m_mutex.lock();
    fputs(single_log.c_str(), m_fp);
    m_mutex.unlock();
  }
}
