#include <gtest/gtest.h>

#include "../../src/lock/lock.h"

static cond _cond;

static locker _mutex;
int con_share_data = 32767;

static void* cond_block(void* arg) {
  while (con_share_data > 0) {
    _mutex.lock();
    con_share_data--;
    _mutex.unlock();
  }
  _cond.broadcast();
}

static bool cond_test() {
  pthread_t thread_id;
  void* exit_status;
  int i;
  pthread_create(&thread_id, NULL, cond_block, NULL);
  _mutex.lock();
  while (con_share_data != 0) {
    _cond.wait(_mutex.get());
  }
  _mutex.unlock();
  return true;
}

TEST(conTest, Basic) {
  EXPECT_EQ(cond_test(), true);
}


int dataReady = 0;
locker _mutex0;
cond _cond0;

static void* producer(void* arg) {
  _mutex0.lock();
  dataReady = 1;
  puts("0000000");
  _cond0.signal();
  puts("1111111");
  _mutex0.unlock();
  puts("2222222");
}
static void* consumer(void* arg) {
  _mutex0.lock();
  while (!dataReady) {
    puts("aaaaaaa");
    _cond0.wait(_mutex0.get());
    puts("bbbbbbb");
  }
  printf("Consumer: Data consumed\n");
  _mutex0.unlock();
}

static bool procon() {
  pthread_t producerThread, consumerThread;

  pthread_create(&consumerThread, NULL, consumer, NULL);
  pthread_create(&producerThread, NULL, producer, NULL);

  pthread_join(producerThread, NULL);
  pthread_join(consumerThread, NULL);
  return true;
}
/** 1.
 * # producer 先走
 * -> lock
 * -> dataReady = 1
 * -> unlock
 * # consumer 后走
 * -> lock
 * -> Data consumed
 * -> unlock
 */
/** 2.
 * # consumer 先走
 * -> lock
 * -> while wait -> 并且unlock
 * # producer 后走
 * -> lock
 * -> dataReady = 1
 * -> signal
 * -> unlock
 * # consumer
 * -> wait 走完
 * -> unlock
 *
 * > 当signal发送条件变量的信号时，他会先释放他所持有的互斥锁，然后再发送信号。这样做的目的时为了确保其他线程能够获得互斥锁并执行相应的操作。
  当信号发出时，signal再次上锁，直到unlock后交出锁。此时wait 拿到锁资源，继续往下走。


 * result: 对应log为:
  aaaaaaa
  0000000
  1111111
  2222222
  bbbbbbb
  Consumer: Data consumed
 */

TEST(cond0Test, Basic) {
  EXPECT_EQ(procon(), 1);
}
