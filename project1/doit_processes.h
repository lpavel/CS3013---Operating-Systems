#ifndef DOIT_H
#define DOIT_H

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


/* definition of a process
 */
typedef struct {
  int pid;
  string processName;
} Process;

/* sets up the input for command line execution
 */
void commandLineSetup(char *argvNew[], int argc, char** argv);

/* sets up the input from the shell for execution 
 */
int  shellLineSetup(char *argvNew[], const string& inStr);

/* collects the data of each process and prints it
 */
void collectData(const struct timeval& initialClockTime,
		 const struct timeval& finalClocktime,
		 const struct rusage&  ruInitial);

/* checks if any processes finished
 */
void checkFinishedProcesses(vector<Process>& pids, 
			    const struct timeval& initialClockTime,
			    const struct rusage&  ruInitial);

/* runs the shell
 */
int shellMode();

/* runs the command given along with the run of doit
 */
int commandExecution(int argc, char**argv);

/* Prints processes currently executing
 */
void printJobs(const vector<Process>& processes);

/* Function that handles changing the directory
 */
void change_dir(char* directory);

#endif
