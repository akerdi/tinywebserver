#include <gtest/gtest.h>

#include "../../src/lock/lock.h"

#define SLEEP_SLOT 0.1

sem _sem;
int _tnum = 10;

static locker _lock;
int _total = 0;

sem _join_wait_sem;
void* join_block(void* arg) {
  printf("111\n");
  sleep(SLEEP_SLOT);
  for (int i = 0 ; i < 5; i++) {
    _sem.post();
  }
  _join_wait_sem.wait();
  printf("333\n");
}

void* sem_block(void* arg) {
  while (true) {
    // 消耗信号量
    _sem.wait();
    _lock.lock();
    _total++;
    printf("_total: %d\n", _total);
    if (_total == 10) {
      printf("222\n");
      _join_wait_sem.post();
    }
    _lock.unlock();
  }
}
bool sem_test() {
  // 因为下方_sem.post()了，所以就不必改为sem(_tnum)了
  _sem = sem(_tnum-5);
  pthread_t* tids;
  // 开启多个异步线程，异步线程会消耗
  tids = new pthread_t[_tnum];
  for (int i = 0 ; i < _tnum; i++) {
    // _sem.post();
    if (pthread_create(tids+i, NULL, sem_block, NULL) != 0) {
      delete [] tids;
      return false;
    }
    if (pthread_detach(tids[i]) != 0) {
      delete [] tids;
      return false;
    }
  }
  pthread_t tid;
  pthread_create(&tid, NULL, join_block, NULL);
  pthread_join(tid, NULL);
  return true;
}

/**
 * -> 5 信号量 -> 10 异步线程 -> 1 同步线程
 *            -> 消耗5 异步  -> 睡眠SLEEP_SLOTs
 * -> 5 信号量 -> 消耗5 异步
 *            -> 计数达到10  -> _join_wait_sem.post
 * 任务结束
*/

TEST(semTest, Basic) {
  EXPECT_EQ(sem_test(), true);
}

///////////////////////
// -------------------
int sharedData = 0;
sem _sem0;
sem _sem1;

static void* producer(void* arg) {
  for (int i = 0; i < 5; i++) {
    _sem0.wait();
    sharedData = i;
    printf("producer signal->\n");
    _sem1.post();
    printf("Producer: Produced data %d\n", sharedData);
  }
}
static void* consumer(void* arg) {
  while (sharedData < 4) {
    _sem0.post();
    printf("Consumer: Waiting for data\n");
    _sem1.wait();
    printf("Consumer index:%d\n", sharedData);
  }
  printf("Consumer: Consumed data %d\n", sharedData);
}

static int procon_test() {
  pthread_t producerThread, consumerThread;
  pthread_create(&producerThread, NULL, producer, NULL);
  pthread_create(&consumerThread, NULL, consumer, NULL);

  pthread_join(producerThread, NULL);
  pthread_join(consumerThread, NULL);

  return 1;
}


TEST(sem0Test, Basic) {
  EXPECT_EQ(procon_test(), 1);
}
