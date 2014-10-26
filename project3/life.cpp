#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <map>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <cerrno>
#include <cstdio>
#include <fstream>
using namespace std;

#define MAXTHREAD 10
#define MAXGRID 40

#define RANGE 1
#define ALLDONE 2
#define GO 3
#define GENDONE 4
#define STOP 5

// the 2 matrices
int before[MAXGRID][MAXGRID];
int after[MAXGRID][MAXGRID];
// global variables related to the game in general
int columnSize;
int lineSize;
int numberOfGenerations;
int numberOfThreadsGlobal;
int barrierCount;

// I typedefed the struct since it is easier to use
typedef struct {
  int iFrom; /* who sent the message (0 .. number-of-threads) */
  int type; /* its type */
  int value1; /* first value */
  int value2; /* second value */
} msg;


sem_t sem_s[MAXTHREAD + 1]; /* semaphores  for send*/
sem_t sem_r[MAXTHREAD + 1]; /* semaphores for receive*/
// semaphores for the barrier
sem_t gendone_sem;
sem_t alldone_sem;

msg mailboxes[MAXTHREAD + 1];

const int parent_threadID = 0;

// function declaration
void sendMsg(int iTo, msg& message);
void recvMsg(int iFrom, msg& message);
int getNumberOfNeighbors(int line, int column);
void printMatrix(int matrix[MAXGRID][MAXGRID]);
void updateMatrix();
void* linesthread(void* arg);
bool isAllZero();
bool hasNotChanged();

int main(int argc, char** argv)
{
  if((argc < 4) || (argc > 6)) {
    cout <<"invalid input - 3,4, or 5 arguments must be given\n";
    cout << "format ./life threads filename generations print input";
    return 1;
  }

  // get the data from the inputs
  int   numberOfThreadsInput = atoi(argv[1]);
  const char* filename       = argv[2];
  const int   generations    = atoi(argv[3]);
  numberOfGenerations = generations;
  if(generations <= 0) {
    cout << "the generations has to be greater than 0\n";
    return 1;
  }
  const char print = (argc > 4)? argv[4][0] : 'n';
  const char input = (argc > 5)? argv[5][0] : 'n';

  const bool isPrint = (print == 'y');
  const bool isInput = (input == 'y');

  // now read the input -- a bit hacky since it reads strings
  // and parses them after into ints
  ifstream inFile(filename);
  int line = 0;
  int column = 0;
  int max_col = 0;
  while(!inFile.eof()) {
    if(line > MAXGRID) {
      cout << "too many lines\n";
      return 1;
    }
    string line_str;
    getline(inFile, line_str);
    char delim[5] = " ,\n\t";
    char *token = strtok(strdup(line_str.c_str()), delim);
    while(token != NULL) {
      if(column > MAXGRID - 1) {
	cout << "too many columns\n";
	return 1;
      }
      before[line][column++] = atoi(token);
      token = strtok(NULL, delim);
      max_col = (max_col < column)? column : max_col;
    }
    column = 0;
    ++line;
  }
  lineSize = line - 1;
  columnSize = max_col;

  inFile.close();

  // now check if the number of threads is higher than
  // the number of lines - if so, set # threads to # lines
  if(numberOfThreadsInput > MAXTHREAD) {
    cout << "Too many threads. The maximum is 10\n";
    exit(1);
  }
  
  const int numberOfThreads = (numberOfThreadsInput > lineSize)
    ? lineSize
    : numberOfThreadsInput;

  
  numberOfThreadsGlobal = numberOfThreads; 
  // keep both of the because the const one allows
  // to declare the array of pthreads of any size
  // wanted. This trick has been done in man places in the
  // project so that it avoids malloc and work with pointers
  pthread_t threads[numberOfThreads + 1];

  // initialize the semaphores for the mailboxes and for the barrier
  for(int i = 0; i <  MAXTHREAD + 1; ++i) {
    if (sem_init(&sem_s[i], 0, 1) < 0) {
      perror("sem_init");
      exit(1);
    }
  }

  for(int i = 0; i <  MAXTHREAD + 1; ++i) {
    if (sem_init(&sem_r[i], 0, 0) < 0) {
      perror("sem_init");
      exit(1);
    }
  }

  if (sem_init(&gendone_sem, 0, 1) < 0) {
    perror("gendone_sem init");
    exit(1);
  }

  if (sem_init(&alldone_sem, 0, 0) < 0) {
    perror("alldone_sem init");
    exit(1);
  }

  // first create the threads - start from 1 for consistency (0 is reserved
  // for the parent thread)
  for(int  i = 1; i <= numberOfThreads; ++i) {
    if (pthread_create(&threads[i], NULL, linesthread, (void *) (intptr_t)i) != 0) {
      perror("pthread_create");
      exit(1);
    }
  }

  int generationNumber = 0;
  
  // first send RANGE to each of them
  // now send the data via mailboxes
  int range = lineSize/numberOfThreads;
  for(int  i = 0; i < numberOfThreads - 1; ++i) {
    msg message;
    message.iFrom = 0;
    message.type = RANGE;
    message.value1 = i * range;
    message.value2 = (i+1) * range - 1;
    sendMsg(i + 1, message);
  }
  // last value takes the rest
  msg message;
  message.iFrom = 0;
  message.type = RANGE;
  message.value1 = (numberOfThreads - 1) * range;
  message.value2 = lineSize - 1;
  sendMsg(numberOfThreads, message);
    
  bool allDone = false;
  int cGen = 0;
  int allDoneCounter = 0;
  while(!allDone) {
    if(isPrint) { // print matrix if user says so
      cout << "Generation " << cGen << ":\n";
      printMatrix(before);
      cout << '\n';
    }

    if(isInput) { // wait for user input if he wants to
      cin.get();
    }

    // now sent GO to each of them
    for(int i = 1; i <= numberOfThreads; ++i) {
      msg goMessage;
      goMessage.type = GO;
      sendMsg(i, goMessage);
    }
    
    // when they all stopped, do the next generation
    int genDoneCounter = 0;

    
    for(int i = 1; i <= numberOfThreads; ++i) {
      msg genDoneMessage;
      recvMsg(parent_threadID, message);
      if(message.type == GENDONE) {
	++genDoneCounter;
      }
      else if(message.type == ALLDONE) {
	++allDoneCounter;
      }
      else {
	cout << "messages sent back improperly\n";
      }
    }
    // all threads finished their job
    if(allDoneCounter == numberOfThreads) {
      break;
    }
    // Here it gets Optimized - sends STOP if there is 
    // nothing more to be computed	
    ++cGen;
    if(isAllZero() || hasNotChanged()) {
      for(int i = 1; i <= numberOfThreads; ++i) {
	msg goMessage;
	goMessage.type = STOP;
	sendMsg(i, goMessage);
      }
      break;
    }
    // update only if it is not the end of the game
    updateMatrix();
    allDone = (allDoneCounter == numberOfThreads);
  }

  cout << "The game ends after " << cGen
       << " generations with:\n";
  printMatrix(after);

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

void printMatrix(int matrix[MAXGRID][MAXGRID]) {
  for(int i = 0; i < lineSize; ++i) {
    for(int j = 0; j < columnSize; ++j) {
      cout << matrix[i][j] << " ";
    }
    cout << '\n'; 
  }
}

void updateMatrix() {
  for(int i = 0; i < lineSize; ++i) {
    for(int j = 0; j < columnSize; ++j) {
      before[i][j] = after[i][j]; 
    }
  }
}

bool isAllZero() {
  bool isZero = true;
  for(int i = 0; i < lineSize; ++i) {
    for(int j = 0; j < columnSize; ++j) {
      if(after[i][j] != 0) {
	isZero = false;
      }
    }
  }
  return isZero;
}

bool hasNotChanged() {
  bool notChanged = true;
  for(int i = 0; i < lineSize; ++i) {
    for(int j = 0; j < columnSize; ++j) {
      if(before[i][j] != after[i][j])
	notChanged = false;
    }
  }
  return notChanged;
}


void* linesthread(void* arg) {
  const int threadID = (intptr_t) arg;
  msg message;
  recvMsg(threadID, message);
  if(message.type != RANGE) {
    cout << "Wrong initial message for thread" << 
      threadID << '\n';
    exit(1);
  }

  int lineFrom = message.value1;
  int lineTo   = message.value2;

  for(int gen = 1; gen <= numberOfGenerations; ++gen) {
    recvMsg(threadID, message);
    // check if it has to stop or go on
    if(message.type == STOP) {
      pthread_exit(0);
    }
    else if(message.type == GO) {
    }
    else {
      cout << "Should receive a GO or STOP message on thread:" << 
	threadID << '\n';
      exit(1);
    }
    // update the state of the game
    for(int line = lineFrom; line <= lineTo; ++line) {
      for(int column = 0; column < columnSize; ++column) {
	int numNeighbors = getNumberOfNeighbors(line, column);
	if((numNeighbors <= 1) || (numNeighbors >= 4))
	  after[line][column] = 0;
	else if(numNeighbors == 3)
	  after[line][column] = 1;
	else if((numNeighbors == 2) && (before[line][column] != 0))
	  after[line][column] = 1;
	else
	  after[line][column] = before[line][column];
      }
    }

    message.type = GENDONE;
    sendMsg(parent_threadID, message);
  }

  // create a barrier here so that no ALLDONEs before GENDONEs
  sem_wait(&gendone_sem);
  barrierCount++;
  sem_post(&gendone_sem);
  
  if(barrierCount == numberOfThreadsGlobal) {
    for(int i = 0; i <= numberOfThreadsGlobal; ++i)
      sem_post(&alldone_sem);
  }
  
  sem_wait(&alldone_sem);
  message.type   = ALLDONE;
  
  sendMsg(parent_threadID, message);
  pthread_exit(0);
}

int getNumberOfNeighbors(int line, int column) {
  int counter = 0;
  if((before[line-1][column-1] != 0)  && (line-1 >= 0) && (column-1 >= 0)) {
    ++counter;
  }
  if((before[line-1][column]   != 0)  && (line-1 >= 0)) {
    ++counter;
  }
  if((before[line-1][column+1] != 0)  && (line-1 >= 0) && (column+1 <= columnSize)) {
    ++counter;
  }
  if((before[line][column-1]   != 0)  && (column-1 >= 0)) {
    ++counter;
  }
  if((before[line][column+1]   != 0)  && (column+1 <= columnSize)) {
    ++counter;
  }
  if((before[line+1][column-1] != 0)  && (line+1 <= lineSize) && (column-1 >= 0)) {
    ++counter;
  }
  if((before[line+1][column]   != 0)  && (line+1 <= lineSize)) {
    ++counter;
  }
  if((before[line+1][column+1] != 0)  && (line+1 <= lineSize) 
     && (column+1 <= columnSize)) {
    ++counter;
  }
  return counter;
}

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
