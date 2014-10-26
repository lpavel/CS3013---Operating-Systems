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


int commandExecution(int argc, char **argv) {
  // data that will be used for printing
  struct timeval initialClockTime;
  struct timeval finalClockTime;
  struct rusage ruInitial;

  char *argvNew[5]; // array of strings for execvp
  int pid; // process id

  // create new process
  if ((pid = fork()) < 0) {
    cerr << "Fork error\n";
    exit(1);
  }
  else if (pid == 0) {
    // child process that runs the command given
    commandLineSetup(argvNew, argc, argv);
    if (execvp(argvNew[0], argvNew) < 0) {
      cerr << argvNew[0] <<": command not found\n";
      exit(1);
    }
  }
  else {
    /* parent */
    // get initial data
    getrusage(RUSAGE_SELF, &ruInitial);
    gettimeofday(&initialClockTime, NULL);
    wait(0);/* wait for the child to finish */
    // get final data
    gettimeofday(&finalClockTime, NULL);
    collectData(initialClockTime, finalClockTime, ruInitial);
  }
  return 0;
}

void change_dir(char* directory) {
  // go to the directory pointed
  if(directory) {
    if(chdir(directory) != 0) {
      cerr << "bash: cd: " << directory << ": " << 
	strerror(errno) << '\n';
    }
  }
  else { // if nothing is specified, go to home
    chdir(getenv("HOME"));
  }
}

int shellMode() {
  // data that will be used for statistics
  struct timeval initialClockTime;
  struct timeval finalClockTime;
  struct rusage ruInitial;
  bool condition = true;

  // vector that keeps track of which processes are being run
  vector<Process> processes; 

  cout << "Welcome to the Shell \n";

  while(condition) {
    char *argvNew[5];          // strings used by execvp
    int pid;                   // process id
    string inStr;              // string inserted from stdin
    bool isBackground = false; // if a variable is run in background
    char directoryStr[1024];   // shows the current directory
    if (getcwd(directoryStr, sizeof(directoryStr)) != NULL) {
      cout << directoryStr << "$ ";
    }
    // get input and check if it runs in the background
    getline(std::cin, inStr);
    if(inStr.at(inStr.length() - 1) == '&') {
      inStr.erase(inStr.length() - 1);
      isBackground = true;
    }

    // get initial data
    gettimeofday(&initialClockTime, NULL);
    getrusage(RUSAGE_SELF, &ruInitial);

    
    if(shellLineSetup(argvNew, inStr) == 1) {
      continue;
    }
    if(inStr == "exit") {
      // check that everything is closed before exiting
      while(processes.size() > 0) {
	checkFinishedProcesses(processes, initialClockTime, ruInitial);
      }
      return 0;
    }
    else if(strcmp(argvNew[0], "cd") == 0) {
      // change dir doesn't fork and happens at the parent level
      change_dir(argvNew[1]);
    }
    else if(strcmp(argvNew[0], "jobs") == 0) {
      // jobs also prints from the parent process
      printJobs(processes);
    }    
    else {
      if ((pid = fork()) < 0) {
	cerr << "Fork error\n";
	exit(1);
      }
      else if (pid == 0) {
	/* child process */
	if (execvp(argvNew[0], argvNew) < 0) {
	  cerr << argvNew[0] <<": command not found\n";
	  exit(1);
	}
      }
      else {
	/* parent */
	// first check if any processes have finished
	checkFinishedProcesses(processes, initialClockTime, ruInitial);
	if(isBackground == true) { // if run in background, show it
	  cout << '[' << processes.size() + 1 << "] " << pid << '\n';
	  Process process;
	  process.pid = pid;
	  process.processName = inStr;
	  processes.push_back(process);
	}
	else { // if not in the background, run it as in command execution
	  wait(0);/* wait for the child to finish */
	  gettimeofday(&finalClockTime, NULL);
	  collectData(initialClockTime, finalClockTime, ruInitial);
	}
      }
    }
  }
}

void checkFinishedProcesses(vector<Process>& processes, 
			    const struct timeval& initialClockTime,
			    const struct rusage&  ruInitial) {
  int status;
  struct timeval finalClockTime; // collects data about the finished process
  const int vector_size = processes.size(); // optimization purposes
  // loop backwards is less error prone when need to delete stuff
  for(int i = vector_size - 1; i >= 0; --i) { 
    // check if the process has finished
    if(waitpid(processes[i].pid, &status, WNOHANG) == processes[i].pid) {
      gettimeofday(&finalClockTime, NULL);
      cout << '[' << i + 1 << "] " << processes[i].pid << 
	" " << processes[i].processName << " Completed" <<'\n';
      // if a process finished, get the data and delete it
      collectData(initialClockTime, finalClockTime, ruInitial);
      processes.erase(processes.begin() + i);
    }
  }
}

int shellLineSetup(char *argvNew[], const string& inStr) {
  const char* separator = " ";

  char *inStrC = strdup(inStr.c_str());
  char *part = strtok(inStrC, separator);
  if(part == NULL) {
    return 1; // the shell usually just ignores empty enters
  }

  string filePath; // the first argument of execvp into a string
  // treat cd and jobs differently
  if(strcmp(part, "cd") == 0) { 
    filePath = string(part);
  }
  else if(strcmp(part, "jobs") == 0) {
    filePath = string(part);
  }
  else {
    filePath = "/bin/" + string(part);
  }
  argvNew[0] = strdup(filePath.c_str());
  // finish the parsing
  for(int i = 1; part != NULL; ++i) {
    part = strtok(NULL, separator);
    argvNew[i] = part;
  }
  return 0;
}

void printJobs(const vector<Process>& processes) {
  for(int i = 0; i < processes.size(); ++i) {
    cout << '[' << i + 1 << "] " << processes[i].pid << " " <<  
      processes[i].processName << '\n';
  }
}

void commandLineSetup(char *argvNew[], int argc, char** argv) {
  string filePath = /*"/bin/" + */string(argv[1]);
  argvNew[0] = strdup(filePath.c_str());
  for(int i = 1; i < argc - 1; ++i) { 
    argvNew[i] = argv[i + 1];
  }
  argvNew[argc - 1] = NULL;
}

void collectData(const struct timeval& initialClockTime,
		 const struct timeval& finalClockTime,
		 const struct rusage&  ruInitial){
  struct timeval tim;
  struct rusage ruFinal;
  getrusage(RUSAGE_SELF, &ruFinal);

  // here get user time
  tim = ruFinal.ru_utime;
  cout << "sec: " << tim.tv_sec << " usec: " << tim.tv_usec << '\n';
  double userTime = ((double)tim.tv_sec * 1000.0) + (double)tim.tv_usec / 1000.0;
  cout  << "user time: " << userTime << " ms" << '\n';

  // here get system time
  tim = ruFinal.ru_stime;
  double systemTime = ((double)tim.tv_sec * 1000.0) + (double)tim.tv_usec / 1000.0;
  cout  << "system time: " << systemTime << " ms" << '\n';

  // here get wall-clock time
  double wallClockTime = 
    ((double)(finalClockTime.tv_sec - initialClockTime.tv_sec) * 1000.0) + 
    ((double)(finalClockTime.tv_usec - initialClockTime.tv_usec) / 1000.0);
  cout  << "clock-time: " << wallClockTime << " ms" << '\n';

  // times preempted involuntarily
  cout << "# times preempted involuntarily: " << 
    ruFinal.ru_nivcsw - ruInitial.ru_nivcsw << '\n';

  // times preempted voluntarily
  cout << "# times preempted voluntarily: " <<
    ruFinal.ru_nvcsw - ruInitial.ru_nvcsw << '\n';

  // number of pagefaults
  cout << "# pagefaults: " <<
    ruFinal.ru_majflt - ruInitial.ru_majflt << '\n';
  
  // number of reclaims
  cout << "# reclaims: " <<
    ruFinal.ru_minflt - ruInitial.ru_minflt << '\n';
}
