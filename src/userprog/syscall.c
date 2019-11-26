/*
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
      thread_exit();

    case SYS_WAIT:
      check_address(stack_pointer+1);
      f->eax = process_wait(*(stack_pointer+1));//pass tid of process to do
      break;

    case SYS_EXEC:
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
*/
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include <threads/vaddr.h>
#include <filesys/filesys.h>
#include <string.h>
#include <filesys/file.h>
#include <devices/input.h>
#include <threads/palloc.h>
#include <threads/malloc.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "pagedir.h"

static void syscall_handler (struct intr_frame *);
static void check_address_multiple(int *p, int num);
static void check_address(void *p);
struct file_search * fd2fp(int fd);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void unexpected_exit(){
  thread_current()->record = -1;
  exit_ ();
}


void check_address_multiple(int *p, int num){
  int *p2=p;
  for(int i=0; i<num; i++,p2++) 
    check_address((void *)p2);
}

void check_address(void *p){

  if(p == NULL||!is_user_vaddr(p)||!pagedir_get_page(thread_current()->pagedir,p)){
    unexpected_exit();
  }
  else{
    return;
  }

}

struct file_search * fd2fp(int fd){

  for (struct list_elem *e = list_begin (&thread_current()->files); e != list_end (&thread_current()->files); e = list_next (e)){
    if (fd==list_entry (e, struct file_search, elem)->fd)
      return list_entry (e, struct file_search, elem);
  }
  return false;
}

void exit_(){

  thread_current()->child->record=thread_current()->record;
  
  sema_up(&thread_current()->child->sema);

  //file_allow_write）
  file_sema_down();
  file_close (thread_current()->self);
  file_sema_up();

  //close all
  for(struct list_elem *e = list_begin(&thread_current()->files);e != list_end(&thread_current()->files);e = list_next(e)){
      file_sema_down();
      file_close (list_entry(e,struct file_search,elem)->fp);
      file_sema_up();
      list_remove(e);
  }

  thread_exit();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int * p =f->esp;
  check_address((void *)p);
  int type=*p++;

  switch (type){
    case SYS_HALT:
      shutdown_power_off();

    case SYS_EXIT:
      check_address(p);
      thread_current()->record = *p;
      exit_ ();

    case SYS_EXEC:
      check_address(p);
      check_address(*p);
      f->eax = process_execute(*p);
      break;

    case SYS_WAIT:
      check_address(p);
      f->eax = process_wait(*p);
      break;

    case SYS_CREATE:
      check_address_multiple(p, 2);
      check_address(*p);
      file_sema_down();
      f->eax = filesys_create(*p,*(p+1));
      file_sema_up();
      break;

    case SYS_REMOVE:
      check_address(p);
      check_address(*p);
      file_sema_down();
      f->eax = filesys_remove(*p);
      file_sema_up();
      break;

    case SYS_OPEN:
      check_address(p);
      check_address(*p);
      struct thread * t=thread_current();
      file_sema_down();
      struct file * temp =filesys_open(*p);
      file_sema_up();
      if(temp != NULL){
        struct file_search *of = malloc(sizeof(struct file_search));
        of->fd = t->increase_file_id_generate;
        t->increase_file_id_generate++;
        of->fp = temp;
        list_push_back(&t->files, &of->elem);
        f->eax = of->fd;
      } else
        f->eax = -1;
      break;

    case SYS_FILESIZE:
      check_address(p);
      struct file_search * temp2 = fd2fp(*p);
      if (temp2!= NULL){
        file_sema_down();
        f->eax = file_length(temp2->fp);
        file_sema_up();
      } else
        f->eax = -1;
      break;

    case SYS_READ:
      check_address_multiple(p, 3);
      check_address((void*)*(p+1));
      int fd = *p;
      uint8_t * buffer = (uint8_t*)*(p+1);
      off_t size = *(p+2);
      if (fd==0) {
        for (int i=0; i<size; i++)
          buffer[i] = input_getc();
        f->eax = size;
      }
      else{
        struct file_search * temp3 = fd2fp(*p);
        if (temp3){
          file_sema_down();
          f->eax = file_read(temp3->fp, buffer, size);
          file_sema_up();
        } else
          f->eax = -1;
      }
      break;

    case SYS_WRITE:
      check_address_multiple(p, 3);
      check_address((void*)*(p+1));
      int fd2 = *p;
      char * buffer2 = *(p+1);
      off_t size2 = *(p+2);;
      if (fd2==1) {
        putbuf(buffer2,size2);
        f->eax = size2;
      }
      else{
        struct file_search * temp4 = fd2fp(*p);
        if (temp4){
          file_sema_down();
          f->eax = file_write(temp4->fp, buffer2, size2);
          file_sema_up();
        } else
          f->eax = 0;
      }
      break;

    case SYS_SEEK:
      check_address_multiple(p, 2);
      struct file_search * temp5 = fd2fp(*p);
      if (temp5){
        file_sema_down();
        file_seek(temp5->fp, *(p+1));
        file_sema_up();
      }
      break;

    case SYS_TELL:
      check_address((void *)p);
      struct file_search * temp6 = fd2fp(*p);
      if (temp6){
        file_sema_down();
        f->eax = file_tell(temp6->fp);
        file_sema_up();
      }else
        f->eax = -1;
      break;

    case SYS_CLOSE:
      check_address((void *)p);
      struct file_search * openf=fd2fp(*p);
      if (openf){
        file_sema_down();
        file_close(openf->fp);
        file_sema_up();
        list_remove(&openf->elem);
        free(openf);
      }
      break;

    default:
      unexpected_exit();
      break;
  }
}