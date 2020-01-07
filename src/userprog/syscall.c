#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "vm/page.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "process.h"
#include "lib/user/syscall.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void exit_(){
  //wake parent
  thread_current()->self_->record=thread_current()->record;
  sema_up(&thread_current()->self_->sema);
  //close self
  file_sema_down();
  file_close (thread_current()->self);
  file_sema_up();
  //close all
  struct list_elem *e;
  for (struct list *files = &thread_current()->files; !list_empty(files);)
  {
    e = list_pop_front(files);
    struct file_search *f = list_entry (e, struct file_search, elem);
    file_sema_down();
    file_close(f->fp);
    file_sema_up();
    list_remove(e);
    free(f);
  }
  thread_exit();
}

void error_exit(){
  thread_current()->record = -1;
  exit_ ();
}

void check_address(void *p, int *stack_pointer)
{
  struct thread *t = thread_current();
  if(p == NULL||!is_user_vaddr(p)){
    error_exit();
  } else if (!pagedir_get_page(t->pagedir,p)){
#ifdef VM
    if((p > stack_pointer || p == stack_pointer - 1 || p == stack_pointer - 8)
    && get_page_entry(t->page_table, p) == NULL){
      Install_new_page(t->page_table, p);
      return;
    }
#endif
    error_exit();
  } else{
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
  check_address(stack_pointer, stack_pointer);
  int system_call = *(stack_pointer);
  //printf("%d\n",system_call);
  switch(system_call){
    case SYS_HALT:
      shutdown_power_off();
      f->eax = 0;

    case SYS_EXIT:
      check_address(stack_pointer + 1, stack_pointer);
      thread_current()->record = *(stack_pointer+1);
      exit_();

    case SYS_WAIT:
      check_address(stack_pointer + 1, stack_pointer);
      f->eax = process_wait(*(stack_pointer+1));//pass tid of process to do
      break;

    case SYS_EXEC:
      check_address(stack_pointer + 1, stack_pointer);
      check_address(*(stack_pointer + 1), stack_pointer);
      f->eax = process_execute(*(stack_pointer+1));//to do
      break;

    case SYS_CREATE:
      check_address(stack_pointer + 1, stack_pointer);//address of address of file head
      check_address(*(stack_pointer + 1), stack_pointer);//address of file head
      check_address(stack_pointer + 2, stack_pointer);//address of file size
      file_sema_down();
      f->eax = filesys_create(*(stack_pointer+1),*(stack_pointer+2));
      file_sema_up();
      break;

    case SYS_REMOVE:
      check_address((stack_pointer + 1), stack_pointer);
      check_address(*(stack_pointer + 1), stack_pointer);//file name
      file_sema_down();
      f->eax = filesys_remove(*(stack_pointer+1));
      file_sema_up();
      break;

    case SYS_READ:
      check_address(stack_pointer + 1, stack_pointer);//fd
      check_address(stack_pointer + 2, stack_pointer);//void *buffer
      check_address(*(stack_pointer + 2), stack_pointer);
      check_address(stack_pointer + 3, stack_pointer);//size
      if(*(stack_pointer+1) == 0)
      {
        uint8_t* buffer = *(stack_pointer+2);
        for(int i = 0;i < *(stack_pointer+3);i++){
            buffer[i] = input_getc();
        }
        f->eax = *(stack_pointer+3);
      }
      else{
        struct file* temp = fd2fp(*(stack_pointer+1));
        if(temp==NULL){
          f->eax = -1;
        }
        else{
          file_sema_down();
#ifdef VM
          buffer_load_and_pin(*(stack_pointer+2),*(stack_pointer+3));
#endif
          f->eax = file_read(temp,*(stack_pointer+2),*(stack_pointer+3));
#ifdef VM
          buffer_unpin(*(stack_pointer+2),*(stack_pointer+3));
#endif
          file_sema_up();
        }
      }

      break;

    case SYS_WRITE:
      //printf ("write file!\n");
      check_address(stack_pointer + 1, stack_pointer);//fd
      check_address(stack_pointer + 2, stack_pointer);//buffer
      check_address(*(stack_pointer + 2), stack_pointer);
      check_address(stack_pointer + 3, stack_pointer);//size
      //printf("%d\n",*(stack_pointer+1));
      //printf("%d\n",*(stack_pointer+3));
      if(*(stack_pointer+1)==1){
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
          file_sema_down();
#ifdef VM
          buffer_load_and_pin(*(stack_pointer+2),*(stack_pointer+3));
#endif
          f->eax = file_write(temp2,*(stack_pointer+2),*(stack_pointer+3));
#ifdef VM
          buffer_unpin(*(stack_pointer+2),*(stack_pointer+3));
#endif
          file_sema_up();
          //printf("a%d\n",f->eax);
        }
      }
      //printf ("write finish!\n");
      break;

    case SYS_OPEN:
      check_address((stack_pointer + 1), stack_pointer);
      check_address(*(stack_pointer + 1), stack_pointer);
      file_sema_down();
      struct file* target_file = filesys_open(*(stack_pointer+1));
      file_sema_up();
      if(target_file != NULL){
        struct file_search* temp1 = malloc(sizeof(struct file_search));
        temp1->fp = target_file;
        temp1->fd = thread_current()->increase_file_id_generate;
        thread_current()->increase_file_id_generate++;
        list_push_back(&thread_current()->files,&temp1->elem);
        f->eax = temp1->fd;
      }
      else{
        f->eax = -1;
      }
      break;

    case SYS_CLOSE:
      check_address(stack_pointer + 1, stack_pointer);//fd
      //to do
      for(struct list_elem *e = list_begin(&thread_current()->files);e != list_end(&thread_current()->files);e = list_next(e)){
        if(list_entry(e,struct file_search,elem)->fd == *(stack_pointer+1)){
          file_sema_down();
          file_close(list_entry(e,struct file_search,elem)->fp);
          file_sema_up();
          list_remove(e);
        }
      }
      break;

    case SYS_FILESIZE:
      check_address(stack_pointer + 1, stack_pointer);//get fd, convert fd to file pointer.
      
      struct file * temp3 = fd2fp(*(stack_pointer+1));
      if(temp3 != NULL){
        file_sema_down();
        f->eax = file_length(temp3);//to do
        file_sema_up();
      }
      else{
        f->eax = -1;
      }
      
      break;

    case SYS_SEEK:
      check_address(stack_pointer + 1, stack_pointer);//fd
      check_address(stack_pointer + 2, stack_pointer);//position
      file_sema_down();
      file_seek(fd2fp(*(stack_pointer+1)),*(stack_pointer+2));//to do
      file_sema_up();
      break;

    case SYS_TELL:
      check_address(stack_pointer + 1, stack_pointer);
      struct file * temp4 = fd2fp(*(stack_pointer+1));
      if(temp4 != NULL){
        file_sema_down();
        f->eax = file_tell(temp4);//to do
        file_sema_up();
      }
      else{
        f->eax = -1;
      }
      break;
#ifdef VM
    case SYS_MMAP:
      check_address(stack_pointer + 1, stack_pointer);//fd
      check_address(stack_pointer + 2, stack_pointer);//*address
      check_address(*(stack_pointer + 2), stack_pointer);
      mapid_t ret = sys_mmap(*(stack_pointer + 1), *(stack_pointer + 2));
      f->eax = ret;
      break;

    case SYS_MUNMAP:
      check_address(stack_pointer + 1, stack_pointer);//mmapid_t
      sys_unmmap(*(stack_pointer + 1));
      break;
#endif
    default:
      error_exit();
      break;
  }
  //printf ("system call!\n");
  //thread_exit ();//........big bug here!!!!!!!!!!!!!!!!!!
}