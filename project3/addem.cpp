#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <map>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <cerrno>
#include <cstdio>
using namespace std;

#define MAXTHREAD 10
#define RANGE 1
#define ALLDONE 2

typedef struct {
  int iFrom; /* who sent the message (0 .. number-of-threads) */
  int type; /* its type */
  int value1; /* first value */
  int value2; /* second value */
} msg;


sem_t sem_s[MAXTHREAD + 1]; /* semaphores  for send*/
sem_t sem_r[MAXTHREAD + 1]; /* semaphores for receive*/
msg mailboxes[MAXTHREAD + 1];
bool isSent[MAXTHREAD + 1];

const int parent_threadID = 0;


void sendMsg(int iTo, msg& message) {
  sem_wait(&sem_s[iTo]);
  memcpy(&mailboxes[iTo], &message, sizeof(msg));
  sem_post(&sem_r[iTo]);
}

void recvMsg(int iFrom, msg& message) {
  sem_wait(&sem_r[iFrom]);
  memcpy(&message, &mailboxes[iFrom], sizeof(msg));
  sem_post(&sem_s[iFrom]);
}

void* sumthread(void* arg) {
  const int threadID = (intptr_t) arg;
  msg message;
  recvMsg(threadID, message);
  int sum = 0;

  for(int value = message.value1; value <= message.value2; ++value) {
    sum += value;
  }

  message.type   = ALLDONE;
  message.value1 = sum;

  sendMsg(parent_threadID, message);
  pthread_exit(0);
}

int main(int argc, char** argv)
{
  if(argc != 3) {
    cout <<"invalid input - 2 arguments must be given\n";
    return 1;
  }

  const int numberOfThreads = atoi(argv[1]);
  if(numberOfThreads <= 0) {
    cout << "you need to specify 1 or more threads\n";
    return 1;
  }

  if(numberOfThreads > MAXTHREAD) {
    cout << "Too many threads. The maximum is 10\n";
    exit(1);
  }
  pthread_t threads[numberOfThreads + 1];
  const int maxValue = atoi(argv[2]);


  // initialize the semaphores for the mailboxes
  for(int i = 0; i <  MAXTHREAD + 1; ++i) {
    if (sem_init(&sem_s[i], 0, 1) < 0) {
      perror("sem_init");
      exit(1);
    }
  }

  // initialize the semaphores for the mailboxes
  for(int i = 0; i <  MAXTHREAD + 1; ++i) {
    if (sem_init(&sem_r[i], 0, 0) < 0) {
      perror("sem_init");
      exit(1);
    }
  }

  // first create the threads - start from 1 for consistency (0 is reserved
  // for the parent thread)
  for(int  i = 1; i <= numberOfThreads; ++i) {
    if (pthread_create(&threads[i], NULL, sumthread, (void *) (intptr_t)i) != 0) {
      perror("pthread_create");
      exit(1);
    }
  }

  // now send the data via mailboxes
  int range = maxValue/numberOfThreads;
  for(int  i = 0; i < numberOfThreads - 1; ++i) {
    msg message;
    message.iFrom = 0;
    message.type = RANGE;
    message.value1 = i * range + 1;
    message.value2 = (i+1) * range;
    sendMsg(i + 1, message);
  }
  // last value takes the rest
  msg message;
  message.iFrom = 0;
  message.type = RANGE;
  message.value1 = (numberOfThreads - 1) * range + 1;
  message.value2 = maxValue;
  sendMsg(numberOfThreads, message);

  int fullSum = 0;

  for(int i = 1; i <= numberOfThreads; ++i) {
    msg message;
    recvMsg(parent_threadID, message);
    if(message.type != ALLDONE) {
      cout << "messages sent back improperly\n";
    }
    fullSum += message.value1;
  }
  
  cout << "The total for 1 to " << maxValue <<" using " 
       << numberOfThreads <<" threads is " << fullSum <<".\n";


  // wait for all threads to finish
  for(int i = 1 ; i <= numberOfThreads; ++i) {
    (void)pthread_join(threads[i], NULL);
  }

  // destroy all the semaphores
  for(int i = 0; i <= numberOfThreads; ++i) {
    (void)sem_destroy(&sem_s[i]);
    (void)sem_destroy(&sem_r[i]);
  }
}        
