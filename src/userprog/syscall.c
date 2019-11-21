#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void unexpect_exit(void){
  thread_current()->record = -1;
  thread_exit();
}


void check_address(void *p){
  if(p == NULL){
    unexpect_exit();
  }
  else if(!is_user_vaddr(p)){
    unexpect_exit();
  }
  else if(!pagedir_get_page(thread_current()->pagedir,p)){
    unexpect_exit();
  }
  else{
    return;
  }
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *stack_pointer = f->esp;
  check_address(stack_pointer);
  int system_call = *(stack_pointer);

  switch(system_call){
    case SYS_HALT:
      shutdown_power_off();
      f->eax = 0;

    case SYS_EXIT:
      check_address(stack_pointer+1);
      thread_current()->record = *stack_pointer;
      thread_exit ();

    case SYS_WAIT:
      check_address(stack_pointer+1);
      f->eax = process_wait(*(stack_pointer+1));//pass tid of process 
      break;

    case SYS_EXEC:
      check_address(stack_pointer+1);
      check_address(*(stack_pointer+1));
      f->eax = process_execute(*(stack_pointer+1));
      break;

  }
  printf ("system call!\n");
  thread_exit ();
}
