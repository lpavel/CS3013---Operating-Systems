#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <vector>
#include "doit_processes.h"
using namespace std;

/* main function
 */
int main(int argc, char **argv)
/* argc -- number of arguments */
/* argv -- an array of strings */
{
  if(argc == 1) { // shell mode
    if(shellMode() != 0) {
	exit(1);
    }
  }
  else { 
    if(commandExecution(argc, argv) != 0) {
      exit(1);
    }
  }
  return 0;
}

