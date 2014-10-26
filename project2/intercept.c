#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <asm/current.h>
#include <asm/errno.h>
#include <linux/sched.h>
#include <linux/time.h>

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

unsigned long **sys_call_table;


asmlinkage long (*ref_sys_cs3013_syscall2)(void);


asmlinkage long new_sys_cs3013_syscall2(struct processinfo* info) {
  struct task_struct *curr_info = current;
  struct processinfo pinfo;

  struct task_struct *task;
  struct list_head *head;

  int tmp_pid1 = -1;
  int tmp_pid2 = -1;

  long long time_ns;
  unsigned long long tmp_st1 = -1;
  unsigned long long tmp_st2 = -1;

  pinfo.state = curr_info->state;
  pinfo.pid = curr_info->pid;
  pinfo.parent_pid = curr_info->parent->pid;
  pinfo.start_time = timespec_to_ns(&(curr_info->start_time));
  pinfo.uid = current_uid();
  pinfo.cutime = 0;
  pinfo.cstime = 0;

  // first loop over all children after the current one
  list_for_each(head, &(curr_info->children)) {
    task = list_entry(head, struct task_struct, sibling);
    pinfo.cutime += cputime_to_usecs(task->utime);
    pinfo.cstime += cputime_to_usecs(task->stime);
    time_ns = timespec_to_ns(&(task->start_time));
    if(tmp_st1 > time_ns) {
      tmp_st1 = time_ns;
      tmp_pid1 = task-> pid;
    }
  }

  // now loop over the children prior to current one
  list_for_each_prev(head, &(curr_info->children)) {
    task = list_entry(head, struct task_struct, sibling);
    pinfo.cutime += cputime_to_usecs(task->utime);
    pinfo.cstime += cputime_to_usecs(task->stime);
    time_ns = timespec_to_ns(&(task->start_time));
    if(tmp_st1 > time_ns) {
      tmp_st1 = time_ns;
      tmp_pid1 = task-> pid;
    }
  }

  pinfo.youngest_child = tmp_pid1;
  tmp_st1 = 0;
  tmp_pid1 = -1;

  list_for_each(head, &(curr_info->sibling)) {
    task = list_entry(head, struct task_struct, sibling);
    time_ns = timespec_to_ns(&(task->start_time));

    if((time_ns < pinfo.start_time) && (tmp_st1 < time_ns)) {
      tmp_st1 = time_ns;
      tmp_pid1 = task->pid;
    }
    else if((time_ns > pinfo.start_time) && (tmp_st2 > time_ns)) {
      tmp_st2 = time_ns;
      tmp_pid2 = task->pid;
    }
  }

  list_for_each_prev(head, &(curr_info->sibling)) {
    task = list_entry(head, struct task_struct, sibling);
    time_ns = timespec_to_ns(&(task->start_time));

    if((time_ns < pinfo.start_time) && (tmp_st1 < time_ns)) {
      tmp_st1 = time_ns;
      tmp_pid1 = task->pid;
    }
    else if((time_ns > pinfo.start_time) && (tmp_st2 > time_ns)) {
      tmp_st2 = time_ns;
      tmp_pid2 = task->pid;
    }
  }

  tmp_pid1 = (tmp_pid1 == 0)
    ? -1
    : tmp_pid1;
  
  pinfo.younger_sibling = tmp_pid2;
  pinfo.older_sibling = tmp_pid1;
  pinfo.user_time = cputime_to_usecs((*curr_info).utime);
  pinfo.sys_time = cputime_to_usecs((*curr_info).stime);
  
  if(copy_to_user(info, &pinfo, sizeof(struct processinfo))) 
    return EFAULT;
  return 1;
}

static unsigned long **find_sys_call_table(void) {
  unsigned long int offset = PAGE_OFFSET;
  unsigned long **sct;
  
  while (offset < ULLONG_MAX) {
    sct = (unsigned long **)offset;

    if (sct[__NR_close] == (unsigned long *) sys_close) {
      printk(KERN_INFO "Interceptor: Found syscall table at address: 0x%02lX",
                (unsigned long) sct);
      return sct;
    }
    
    offset += sizeof(void *);
  }
  
  return NULL;
}

static void disable_page_protection(void) {
  /*
    Control Register 0 (cr0) governs how the CPU operates.

    Bit #16, if set, prevents the CPU from writing to memory marked as
    read only. Well, our system call table meets that description.
    But, we can simply turn off this bit in cr0 to allow us to make
    changes. We read in the current value of the register (32 or 64
    bits wide), and AND that with a value where all bits are 0 except
    the 16th bit (using a negation operation), causing the write_cr0
    value to have the 16th bit cleared (with all other bits staying
    the same. We will thus be able to write to the protected memory.

    It's good to be the kernel!
   */
  write_cr0 (read_cr0 () & (~ 0x10000));
}

static void enable_page_protection(void) {
  /*
   See the above description for cr0. Here, we use an OR to set the 
   16th bit to re-enable write protection on the CPU.
  */
  write_cr0 (read_cr0 () | 0x10000);
}

static int __init interceptor_start(void) {
  /* Find the system call table */
  if(!(sys_call_table = find_sys_call_table())) {
    /* Well, that didn't work. 
       Cancel the module loading step. */
    return -1;
  }
  
  /* Store a copy of all the existing functions */
  ref_sys_cs3013_syscall2 = (void *)sys_call_table[__NR_cs3013_syscall2];

  /* Replace the existing system calls */
  disable_page_protection();

  sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)new_sys_cs3013_syscall2;
  
  enable_page_protection();
  
  /* And indicate the load was successful */
  printk(KERN_INFO "Loaded interceptor!");

  return 0;
}

static void __exit interceptor_end(void) {
  /* If we don't know what the syscall table is, don't bother. */
  if(!sys_call_table)
    return;
  
  /* Revert all system calls to what they were before we began. */
  disable_page_protection();
  sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)ref_sys_cs3013_syscall2;
  enable_page_protection();

  printk(KERN_INFO "Unloaded interceptor!");
}

MODULE_LICENSE("GPL");
module_init(interceptor_start);
module_exit(interceptor_end);
