#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

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

struct file* fd2fp(int fd){
  for(struct list_elem *e = list_begin(&thread_current()->files);e != list_end(&thread_current()->files);e = list_next(e)){
        //printf("b%d\n",list_entry(e,struct file_search,elem)->fd);
        if(list_entry(e,struct file_search,elem)->fd == fd){
          return list_entry(e,struct file_search,elem)->fp;
        }
    }
  return NULL;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *stack_pointer = f->esp;
  check_address(stack_pointer);
  int system_call = *(stack_pointer);
  //printf("%d\n",system_call);
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
      f->eax = process_wait(*(stack_pointer+1));//pass tid of process to do
      break;

    case SYS_EXEC:
      //printf("SYS_EXEC,\n");
      check_address(stack_pointer+1);
      check_address(*(stack_pointer+1));
      f->eax = process_execute(*(stack_pointer+1));//to do
      break;

    case SYS_CREATE:
      check_address(stack_pointer+1);//address of address of file head
      check_address(*(stack_pointer+1));//address of file head
      check_address(stack_pointer+2);//address of file size
      file_sema_down();
      f->eax = filesys_create(*(stack_pointer+1),*(stack_pointer+2));
      file_sema_up();
      break;

    case SYS_REMOVE:
      check_address((stack_pointer+1));
      check_address(*(stack_pointer+1));//file name
      file_sema_down();
      f->eax = filesys_remove(*(stack_pointer+1));
      file_sema_up();
      break;

    case SYS_READ:
      check_address(stack_pointer+1);//fd
      check_address(stack_pointer+2);//void *buffer 
      check_address(*(stack_pointer+2));
      check_address(stack_pointer+3);//size
      file_sema_down();
      if(*(stack_pointer+1)==0)
      {
        int i;
        uint8_t* buffer = *(stack_pointer+2);
        for(i=0;i<*(stack_pointer+3);i++)
          buffer[i] = input_getc();
        f->eax = *(stack_pointer+3);
      }
      else{
        struct file* temp = fd2fp(*(stack_pointer+1));
        if(temp==NULL){
          f->eax = -1;
        }
        else{
          f->eax = file_read(temp,*(stack_pointer+2),*(stack_pointer+3));
        }
      }
      file_sema_up();
      break;

    case SYS_WRITE:
      //printf ("write file!\n");
      check_address(stack_pointer+1);//fd
      check_address(stack_pointer+2);//buffer
      check_address(*(stack_pointer+2));
      check_address(stack_pointer+3);//size
      file_sema_down();
      //printf("%d\n",*(stack_pointer+1));
      //printf("%d\n",*(stack_pointer+3));
      if(*(stack_pointer+1)==1)
      {
        putbuf(*(stack_pointer+2),*(stack_pointer+3));
        f->eax = *(stack_pointer+3);
      }
      else{
        struct file* temp2 = fd2fp(*(stack_pointer+1));
        if(temp2==NULL){
          //printf("no find\n");
          f->eax = -1;
        }
        else{
          f->eax = file_write(temp2,*(stack_pointer+2),*(stack_pointer+3));
          //printf("a%d\n",f->eax);
        }
      }
      file_sema_up();
      //printf ("write finish!\n");
      break;

    case SYS_OPEN:
      check_address((stack_pointer+1));
      check_address(*(stack_pointer+1));
      file_sema_down();
      struct file* target_file = filesys_open(*(stack_pointer+1));
      struct file_search* temp1 = malloc(sizeof(*temp1));
      temp1->fp = target_file;
      temp1->fd = thread_current()->increase_file_id_generate;
      thread_current()->increase_file_id_generate++;
      list_push_back(&thread_current()->files,&temp1->elem);
      file_sema_up();
      break;

    case SYS_CLOSE:
      check_address(stack_pointer+1);//fd
      file_sema_down();
      //to do
      for(struct list_elem *e = list_begin(&thread_current()->files);e != list_end(&thread_current()->files);e = list_next(e)){
        if(list_entry(e,struct file_search,elem)->fd == *(stack_pointer+1)){
          file_close(list_entry(e,struct file_search,elem)->fp);
          list_remove(e);
        }
      }
      file_sema_up();
      break;

    case SYS_FILESIZE:
      check_address(stack_pointer+1);//get fd, convert fd to file pointer.
      file_sema_down();
      f->eax = file_length(fd2fp(*(stack_pointer+1)));//to do
      file_sema_up();
      break;

    case SYS_SEEK:
      check_address(stack_pointer+1);//fd
      check_address(stack_pointer+2);//position
      file_sema_down();
      file_seek(fd2fp(*(stack_pointer+1)),*(stack_pointer+2));//to do
      file_sema_up();
      break;

    case SYS_TELL:
      check_address(stack_pointer+1);
      file_sema_down();
      f->eax = file_tell(fd2fp(*(stack_pointer+1)));//to do
      file_sema_up();
      break;
  }
  //printf ("system call!\n");
  thread_exit ();
}
