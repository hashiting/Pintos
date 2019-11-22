#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "process.h"
#include "filesys/filesys.h"

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
  if(p == NULL||!is_user_vaddr(p)||!pagedir_get_page(thread_current()->pagedir,p)){
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
  printf("%d\n",system_call);
  switch(system_call){
    case SYS_HALT:
      shutdown_power_off();
      f->eax = 0;

    case SYS_EXIT:
      check_address(stack_pointer+1);
      thread_current()->record = *(stack_pointer+1);
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

    case SYS_CREATE:
      check_address(stack_pointer+4);
      check_address(*(stack_pointer+4));
      check_address(stack_pointer+5);
      file_sema_down();
      f->eax = filesys_create(*(stack_pointer+4),*(stack_pointer+5));
      file_sema_up();
      break;

    case SYS_REMOVE:
      check_address((stack_pointer+1));
      check_address(*(stack_pointer+1));
      file_sema_down();
      f->eax = filesys_remove(*(stack_pointer+1));
      file_sema_up();
      break;

    case SYS_READ:
      check_address(stack_pointer+5);
      check_address(stack_pointer+6);
      check_address(*(stack_pointer+6));
      check_address(stack_pointer+7);
      file_sema_down();
      //to do
      file_sema_up();
      break;

    case SYS_WRITE:
      printf ("write file!\n");
      check_address(stack_pointer+5);
      check_address(stack_pointer+6);
      check_address(*(stack_pointer+6));
      check_address(stack_pointer+7);
      file_sema_down();
      //to do
      file_sema_up();
      printf ("write finish!\n");
      break;

    case SYS_OPEN:
      check_address((stack_pointer+1));
      check_address(*(stack_pointer+1));
      file_sema_down();
      //to do
      file_sema_up();
      break;

    case SYS_CLOSE:
      check_address(stack_pointer+1);
      file_sema_down();
      //to do
      file_sema_up();
      break;

    case SYS_FILESIZE:
      check_address(stack_pointer+1);
      file_sema_down();
      f->eax = file_length(*(stack_pointer));//to do
      file_sema_up();
      break;

    case SYS_SEEK:
      check_address(stack_pointer+5);
      file_sema_down();
      //to do
      file_sema_up();
      break;

    case SYS_TELL:
      check_address(stack_pointer+1);
      file_sema_down();
      //to do
      file_sema_up();
      break;
  }
  printf ("system call!\n");
  thread_exit ();
}
