#include <gtest/gtest.h>

#include "../../src/lock/lock.h"

TEST(HelloTest, BasicAssertions) {
  EXPECT_STRNE("hello", "world");
  EXPECT_EQ(7*6, 42);
}

static locker lock;

int share_data;
static void* block(void* arg) {
  int i;
  for (i = 0; i < 1024 * 1024; i++) {
    lock.lock();
    share_data++;
    lock.unlock();
  }
}

static void thread_test() {
  pthread_t pid;
  pthread_create(&pid, NULL, block, NULL);
  for (int i = 0; i < 50; i++) {
    lock.lock();
    printf("share_data: %d\n", share_data);
    lock.unlock();
  }
  pthread_join(pid, NULL);
}


TEST(mutexTest, Basic) {
  thread_test();
  // lock锁住共享数据，防止抢读抢写
  EXPECT_EQ(1, 1*1);
}