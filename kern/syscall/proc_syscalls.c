#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#include <limits.h>
#include <clock.h>
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include "opt-A2.h"


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);
  #if OPT_A2
    if(curproc->p_parent != NULL){
    spinlock_acquire(&curproc->p_parent->p_lock);
      for (unsigned int temp = 0; temp < array_num(curproc->p_parent->p_children); temp++) {
          struct proc *single_procchild = array_get(curproc->p_parent->p_children, temp);
          if (curproc->p_pid == single_procchild->p_pid) {
            single_procchild->p_exitcode = exitcode;
            break;
          }
        }
        spinlock_release(&curproc->p_parent->p_lock);
    }
  #else
    /* if this is the last user process in the system, proc_destroy()
      will wake up the kernel menu thread */
    proc_destroy(p);
  #endif
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #if OPT_A2
    *retval = curproc->p_pid;
  #else
    *retval = 1;
  #endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}


/*
  executing from the entrypoint function in the kernel
*/
void thread_entrypoint(void *, unsigned long);
void thread_entrypoint(void *ptr, unsigned long abandon){
  (void)abandon;
  enter_forked_process((struct trapframe *) ptr);
}

/*
sys_fork
*/
int
sys_fork(struct trapframe *tf, pid_t *retval){
  /* 
    Call proc_create_runprogram to create a new proc struct in sys_fork
  */
  struct proc* child = proc_create_runprogram(curproc->p_name);
  KASSERT(child != NULL);
  /* 
    call as_copy to copy the address space of the current proces and assign it to the newly created proc struct
     use curproc_getas() to get the address space of the current process
  */
  as_copy(curproc_getas(), &(child->p_addrspace));
  
  /*
    Allocate a new trapframe using kmalloc and copy the trapframe of curproc into it
  */

  struct trapframe *trapframe_for_child = kmalloc(sizeof(struct trapframe));
  *trapframe_for_child = *tf;
  thread_fork(child->p_name, child, thread_entrypoint, trapframe_for_child, 0);
  *retval = child->p_pid;
  clocksleep(1);
  /*
   returns 0 for child process
   */
  return(0);
}
