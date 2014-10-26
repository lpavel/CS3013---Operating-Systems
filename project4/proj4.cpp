#include<iostream>
using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>
#include <semaphore.h>
#include <pthread.h>

#define MAXTHREAD 16

#define RANGE 1
#define ALLDONE 2

typedef struct {
  int iFrom; /* who sent the message (0 .. number-of-threads) */
  int type; /* its type */
  int value1; /* first value */
  int value2; /* second value */
  int value3; /* third value */
  int value4; /* fourth value */
} msg;

sem_t sem_s[MAXTHREAD + 1]; /* semaphores  for send*/
sem_t sem_r[MAXTHREAD + 1]; /* semaphores for receive*/
// semaphores for the barrier
sem_t length_sem;
sem_t alldone_sem;

int numberOfThreadsGlobal;
int barrierCount;
int file_size;
char* pchFile;

msg mailboxes[MAXTHREAD + 1];

const int parent_threadID = 0;


//function declaration
void sendMsg(int iTo, msg& message);
void recvMsg(int iFrom, msg& message);
int regular_read(const int& BUFSIZE, char* filename,
		 int& str_length, int& num_stirngs, int& max_length);
int mmap_read(char*filename, const int& numberOfThreads, 
		 int& str_length, int& num_strings, int& max_length);
int mmap_regular_read(char* filename, int& str_length, 
		      int& num_stirngs, int& max_length);
void* sumthread(void* arg);

int main(int argc, char** argv){
  // check input
  if((argc < 2) || (argc > 3)) {
    cout << "invalid input. ./proj4 <file> o1\n";
    exit(1);
  }

  int numberOfThreads = 1;
  bool isMultiThreaded = false; 
  bool ismmap = false;
  int buf_size = -1;
  struct stat sb;

  // first check for multithreaded
  if(argc == 3) {
    if(strcmp(argv[2] , "mmap") == 0) 
      ismmap = true;

    if(argv[2][0] == 'p') {
      
      isMultiThreaded = true;
      ismmap = true;
      numberOfThreads = atoi(argv[2] + 1);
      
      if(numberOfThreads <= 0) {
	cout << "you need to specify 1 or more threads\n";
	return 1;
      }
      
      if(numberOfThreads > MAXTHREAD) {
	cout << "Too many threads. The maximum is 16\n";
	exit(1);
      } 
    }
    if(isdigit(argv[2][0]))  {
      buf_size = atoi(argv[2]);
      if(buf_size <= 0) {
	cout << "Buffer needs to be bigger than 0\n";
	exit(1);
      }
    }
  }

  const int BUFSIZE = (buf_size > 0)? buf_size : 1024;
  char buf[BUFSIZE]; memset(buf, '\0', BUFSIZE);
  int num_strings = 0;
  pthread_t threads[numberOfThreads + 1];  
  
  if((BUFSIZE > 8192) ) {
    cout << "buffer is too big. Max Buffer size allowed is 8192\n";
    exit(1);
  }
    
  char* fileName = strdup(argv[1]);
  int str_length = 0;
  int max_length = -1;
  
  if(!ismmap) {
    file_size = regular_read(BUFSIZE, fileName, str_length, num_strings, max_length);
  }
  else {
    if(isMultiThreaded) {
      file_size = mmap_read(fileName, numberOfThreads, str_length, num_strings, max_length);
    }
    else {
      file_size = mmap_regular_read(fileName, str_length, num_strings, max_length);
    }
  }
  
  cout << "File size: " << file_size << " bytes.\n";
  cout << "Strings of at least length 4: " << num_strings << '\n';
  cout << "Maximum string length: " << max_length << " bytes\n";
}

int mmap_read(char* fileName, const int& numberOfThreads, 
	      int& str_length, int& num_strings, int& max_length) {

  int fdIn; struct stat sb;
  // open the file
  if ((fdIn = open(fileName, O_RDONLY)) < 0) {
    cerr << "file open\n";
    exit(1);
  }
  
  // get info about file
  if(fstat(fdIn, &sb) < 0){
    perror("Could not stat file to obtain its size");
    exit(1);
  }

  file_size = sb.st_size;

   numberOfThreadsGlobal = numberOfThreads; 
  // keep both of the because the const one allows
  // to declare the array of pthreads of any size
  // wanted. This trick has been done in man places in the
  // project so that it avoids malloc and work with pointers
  pthread_t threads[numberOfThreads + 1];
  int fileSize = sb.st_size;
  

  if ((pchFile = (char *) mmap (NULL, fileSize, PROT_READ, MAP_SHARED, fdIn, 0)) 
      == (char *) -1){
    perror("Could not mmap file");
    exit (1);
  }    

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

  // first create the threads - start from 1 for consistency (0 is reserved
  // for the parent thread)
  for(int  i = 1; i <= numberOfThreads; ++i) {
    if (pthread_create(&threads[i], NULL, sumthread, (void *) (intptr_t)i) != 0) {
      perror("pthread_create");
      exit(1);
    }
  }

  // first send RANGE to each of them
  // now send the data via mailboxes
  int range = fileSize/numberOfThreads;
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
  message.value2 = fileSize - 1;
  sendMsg(numberOfThreads, message);
  num_strings = 0;
  // use an interesting dynamic programming algorithm that computes the maximum length
  int maxLengthEnd[17]; // stores the max Length at the end of a thread string
  int maxStringLength[17]; // stores the maxLength in the particular thread
  for(int i = 0; i < 17; maxStringLength[i] = 0, maxLengthEnd[i++] = 0); // initialize maxLength
  int previous_end = 0;

  // instead of using semaphores for the barrier, 
  // just wait for all threads to finish before doing more computation
  for(int i = 1 ; i <= numberOfThreads; ++i) {
    (void)pthread_join(threads[i], NULL);
  }
  for(int i = 1; i <= numberOfThreads; ++i) {
    msg genDoneMessage;
    recvMsg(i, message);
    // first make sure the message is correct
    if(message.type == ALLDONE) {
      // smush data from threads
      int numberOfStrings  = message.value1;
      int charsAtBegin     = message.value2;
      int charsAtEnd       = message.value3;
      int maxLengthProv    = message.value4;
      num_strings += numberOfStrings;
      // if whole information needs to be combined
      if((numberOfStrings <= 1) && 
	 ((charsAtBegin == charsAtEnd) && (charsAtBegin != 0))) {
	maxLengthEnd[i] = maxLengthEnd[i-1] + charsAtBegin; 
	maxStringLength[i] = maxLengthEnd[i];
	max_length = (maxStringLength[i] > max_length)? maxStringLength[i] : max_length;
	if(previous_end >= 4)
	  --num_strings;
	previous_end = charsAtEnd;
	continue;
      }
      else {
	maxLengthEnd[i] = charsAtEnd;
	int mergedLength = maxLengthEnd[i-1] + charsAtBegin;
	maxStringLength[i] = (mergedLength > maxLengthProv)? mergedLength : maxLengthProv;
	max_length = (maxStringLength[i] > max_length)? maxStringLength[i] : max_length;
	if((previous_end + charsAtBegin >= 4) && 
	   ((charsAtBegin < 4) && (previous_end < 4))) {
	  ++num_strings;
	}
	else if(previous_end >= 4 && charsAtBegin >= 4) {
	  --num_strings;
	}
      }
      previous_end = charsAtEnd;
    }
    else {
      cout << "messages sent back improperly\n";
      cout << "type: "   << message.type   << "\n";
      cout << "value1: " << message.value1 << "\n";
      cout << "value2: " << message.value2 << "\n";
      cout << "value3: " << message.value3 << "\n";
    }
  }
  
  // destroy all the semaphores
  for(int i = 0; i <= numberOfThreads; ++i) {
    (void)sem_destroy(&sem_s[i]);
    (void)sem_destroy(&sem_r[i]);
  }

  // Now clean up
  if(munmap(pchFile, file_size) < 0){
    perror("Could not unmap memory");
    exit(1);
  }

  if (fdIn > 0)
    close(fdIn);

  return sb.st_size;
}

void* sumthread(void* arg) {
  const int threadID = (intptr_t) arg;
  msg message;
  recvMsg(threadID, message);
  if(message.type != RANGE) {
    cout << "Wrong initial message for thread" << 
      threadID << '\n';
    exit(1);
  }

  // get the data needed for computation
  const int byteFrom = message.value1;
  const int byteTo   = message.value2;
  const int virtualSize = byteTo - byteFrom;
  
  // prepare variables that need to send info
  bool isBeginning = true;
  int charsAtBegin = 0;
  int charsAtEnd   = 0;
  int num_strings  = 0;
  int str_length   = 0;
  int max_length   = 0;
  // go through destinated section and compute
  for(int i = byteFrom; i <= byteTo; ++i) {
    char c = pchFile[i];      
    if(isspace(c) || isprint(c)) {
      if(isBeginning)
	++charsAtBegin;
      ++str_length;

      max_length = (max_length < str_length)? str_length : max_length;
    }
    else {
      isBeginning = false;
      str_length = 0;
    }
    if(str_length == 4) {
      ++num_strings;
    }
  }

  charsAtEnd = str_length;
  // send messages back
  message.value1 = num_strings;
  message.value2 = charsAtBegin;
  message.value3 = charsAtEnd;
  message.value4 = max_length;
  message.type   = ALLDONE;
  
  sendMsg(threadID, message);

  pthread_exit(0);
}

// for some bizare reason, this function doesn't work
// same syntax in the main works.
int regular_read(const int& BUFSIZE, char* filename,
		 int& str_length, int& num_strings, int& max_length) {

  int fdIn;
  struct stat sb;

  if ((fdIn = open(filename, O_RDONLY)) < 0) {
    cerr << "file open\n";
    exit(1);
  }
  // get info about file
  if(fstat(fdIn, &sb) < 0){
    perror("Could not stat file to obtain its size");
    exit(1);
  }
	  
  int cnt;
  char buf[BUFSIZE]; memset(buf, '\0', BUFSIZE);
  int bufsize = BUFSIZE;
  // regular read of the file using buffer
  while ((cnt = read(fdIn, buf, bufsize)) > 0) {
    for(int i = 0; i < BUFSIZE; ++i) {
      if(isspace(buf[i]) || isprint(buf[i])) {
	++str_length;
	max_length = (max_length < str_length)? str_length : max_length;
      }
      else {
	str_length = 0;
      }
      
      if(str_length == 4) {
	++num_strings;
      }
    }
    // need to do this so that the ending does not have trash
    memset(buf, '\0', BUFSIZE);
  }
  if (fdIn > 0)
    close(fdIn);
  return sb.st_size;
}

int mmap_regular_read(char* filename, int& str_length, 
		      int& num_strings, int& max_length) {

  int fdIn;
  struct stat sb;
  if ((fdIn = open(filename, O_RDONLY)) < 0) {
    cerr << "file open\n";
    exit(1);
  }
  
  // get info about file
  if(fstat(fdIn, &sb) < 0){
    perror("Could not stat file to obtain its size");
    exit(1);
  }

  int fileSize = sb.st_size;
  if ((pchFile = (char *) mmap (NULL, fileSize, PROT_READ, MAP_SHARED, fdIn, 0)) 
      == (char *) -1){
    perror("Could not mmap file");
    exit (1);
  }    

  // loop through chars and compute
  for(int i = 0; i < fileSize; ++i) {
    char c = pchFile[i];
    if(isspace(c) || isprint(c)) {
      ++str_length;
      max_length = (max_length < str_length)? str_length : max_length;
    }
    else {
      str_length = 0;
    }

    if(str_length == 4) {
      ++num_strings;
    }
  }

  // Now clean up
  if(munmap(pchFile, fileSize) < 0){
    perror("Could not unmap memory");
    exit(1);
  }
  if (fdIn > 0)
    close(fdIn);
  return sb.st_size;
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
