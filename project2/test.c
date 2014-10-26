#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define __NR_cs3013_syscall2 350

struct processinfo {
  long state; // current state of process
  pid_t pid; // process ID of this process
  pid_t parent_pid; // process ID of parent
  pid_t youngest_child; // process ID of youngest child
  pid_t younger_sibling; // pid of next younger sibling
  pid_t older_sibling; // pid of next older sibling
  uid_t uid; // user ID of process owner
  long long start_time; // process start time in
                        // nanoseconds since boot time
  long long user_time; // CPU time in user mode (microseconds)
  long long sys_time; // CPU time in system mode (microseconds
  long long cutime; // user time of children (microseconds)
  long long cstime; // system time of children (microseconds)
}; // struct processinfo

long test_call(struct processinfo* pinfo) {
  return (long) syscall(__NR_cs3013_syscall2, pinfo);
}

void print_info(char* description, struct processinfo* pinfo) {
  printf("--------------------------------------------\n");
  printf("Description of operation: %s\n", description);
  printf("state: %ld\n",           pinfo->state);
  printf("pid: %d\n",             pinfo->pid);
  printf("parent_pid: %d\n",      pinfo->parent_pid);
  printf("youngest_child: %d\n",  pinfo->youngest_child);
  printf("younger_sibling: %d\n", pinfo->younger_sibling);
  printf("older_sibling: %d\n",   pinfo->older_sibling);
  printf("start time: %lld\n",    pinfo->start_time);
  printf("user time: %lld\n",     pinfo->user_time);
  printf("sys time: %lld\n",      pinfo->sys_time);
  printf("cutime: %lld\n",        pinfo->cutime);
  printf("cstime: %lld\n",        pinfo->cstime);
  printf("--------------------------------------------\n\n");
}

int main() {
  struct processinfo *pinfo = malloc(sizeof(struct processinfo));
  int status;
  int pid = fork();
  if(pid != 0) {
    test_call(pinfo);
    print_info("Parent Process begin", pinfo);
    waitpid(pid, &status, 0);
  }
  else {
    int childpid = fork();
    if(childpid != 0) {
      test_call(pinfo);
      print_info("Child process", pinfo);
      waitpid(childpid, status, 0);
    }
    else {
      sleep(4);
      test_call(pinfo);
      print_info("child of child process", pinfo);
    }
  }

  free(pinfo);
  return 0;
}
