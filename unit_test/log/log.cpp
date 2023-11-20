#include <gtest/gtest.h>

#include "../../src/log/log.h"

static int m_close_log = false;


static bool log_test() {
  Log::get_instance()->init("./Service.log", m_close_log, 2000, 800000000, 200);
  LOG_DEBUG("%s", "log test");
  sleep(0.1);
  return true;
}

TEST(logTest, Basic) {
  EXPECT_EQ(log_test(), 1);
}
