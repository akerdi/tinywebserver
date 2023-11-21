#include <gtest/gtest.h>

#include "../../src/timer/lst_timer.h"

sort_timer_lst lst;

int result = 100;
static void cb_func(client_data* user_data) {
  result = -1;
}

static bool lst_timerTest() {
  timer_item* item = new timer_item;
  item->cb_func = cb_func;
  item->expire = 100;
  lst.add_timer(item);
  return true;
}

TEST(lst_timerTest, Basic) {
  EXPECT_EQ(lst_timerTest(), 1);
  // 执行超时检测，cb_func 会将result 设置为-1(因为超时了)
  lst.tick();
  EXPECT_EQ(result, -1);
}
