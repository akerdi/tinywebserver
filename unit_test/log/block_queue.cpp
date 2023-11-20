#include <gtest/gtest.h>
#include <string>

#include "../../src/log/block_queue.h"

/**
 * 这是一个单锁示例，log如下:
 * p: push:0
 * p: end push
 * c: wait
 * c: -
 * c: wait
 * c: -
 * c: wait
 * p: end sleep
 * p: push:1
 * p: end push
 * c: -
 * c: wait
 * c: -
 * c: wait
 * c: -
 * c: wait
 * p: end sleep
 * p: push:2
 * p: end push
 * c: -
 * c: wait
 * c: -
 * c: wait
 * c: -
 * c: wait
 * p: end sleep
 * p: push:3
 * p: end push
 * ...
*/

int total = 10;
int producer_num = 0;
int consumer_num = 0;

block_queue<std::string>* queue;

static void* producer_block(void* arg) {
  char* msg = (char*)malloc(sizeof(char)*3);
  while (total > 0) {
    sprintf(msg, "%d", producer_num);
    printf("p: push:%s\n", msg);
    queue->push(std::string(msg));
    printf("p: end push\n");
    sleep(2);
    printf("p: end sleep\n");
    producer_num++;
    total--;
  }
}
static void* consumer_block(void* arg) {
  std::string a;
  while (total > 0) {
    printf("c: wait\n");
    if (queue->pop(a, 1300)) {
      printf("c: %s\n", a.c_str());
      consumer_num++;
    } else {
      printf("c: -\n");
    }
  }
}

static bool block_queue_test() {
  queue = new block_queue<std::string>(100);
  pthread_t producer_tid, consumer_tid;
  pthread_create(&producer_tid, NULL, producer_block, NULL);
  pthread_create(&consumer_tid, NULL, consumer_block, NULL);
  pthread_join(producer_tid, NULL);
  pthread_join(consumer_tid, NULL);
  if (queue) {
    delete queue;
  }
  return true;
}


TEST(block_queueTest, Basic) {
  EXPECT_EQ(block_queue_test(), 1);
}
