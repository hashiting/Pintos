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
#include "filesys/inode.h"
#include "filesys/directory.h"

static void syscall_handler (struct intr_frame *);

struct file_search* fd2fs(int fd){
  for(struct list_elem *e = list_begin(&thread_current()->files);e != list_end(&thread_current()->files);e = list_next(e)){
        //printf("b%d\n",list_entry(e,struct file_search,elem)->fd);
        if(list_entry(e,struct file_search,elem)->fd == fd){
          if(list_entry(e,struct file_search,elem)->dir != NULL)
            return list_entry(e,struct file_search,elem);
          else
            return NULL;
        }
    }
  return NULL;
}

bool sys_chdir(char* name){
  struct dir *dir = dir_open_path (name);
  if(dir == NULL) return false;
  dir_close (thread_current()->cwd);
  thread_current()->cwd = dir;
  return true;
}

bool sys_readir(int fd,char* name){
  struct file_search* temp = fd2fs(fd);
  if(temp == NULL) return false;
  struct inode *inode;
  inode = file_get_inode(temp->fp);
  if(inode == NULL||!in_dir(inode)) return false;

  ASSERT(temp->dir != NULL);

  return dir_readdir(temp->dir,name);
}


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_address(void *p){
  if(p == NULL||!is_user_vaddr(p)||!pagedir_get_page(thread_current()->pagedir,p)){
    error_exit();
  }
  else{
    return;
  }
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
  if(thread_current()->cwd){
    dir_close(thread_current()->cwd);
  }
  thread_exit();
}

void error_exit(){
  thread_current()->record = -1;
  exit_ ();
}

struct file* fd2fp(int fd,int f){
  for(struct list_elem *e = list_begin(&thread_current()->files);e != list_end(&thread_current()->files);e = list_next(e)){
        //printf("b%d\n",list_entry(e,struct file_search,elem)->fd);
        if(list_entry(e,struct file_search,elem)->fd == fd){
          if(f == 0)
            return list_entry(e,struct file_search,elem)->fp;
          else if(f == 1){
            if(list_entry(e,struct file_search,elem)->dir != NULL)
              return list_entry(e,struct file_search,elem)->fp;
            else
              return NULL;
          }
          else{
            if(list_entry(e,struct file_search,elem)->dir == NULL)
              return list_entry(e,struct file_search,elem)->fp;
            else
              return NULL;
          }
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
      exit_();

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
      f->eax = filesys_create(*(stack_pointer+1),*(stack_pointer+2),false);
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
      if(*(stack_pointer+1) == 0)
      {
        uint8_t* buffer = *(stack_pointer+2);
        for(int i = 0;i < *(stack_pointer+3);i++){
            buffer[i] = input_getc();
        }
        f->eax = *(stack_pointer+3);
      }
      else{
        struct file* temp = fd2fp(*(stack_pointer+1),2);
        if(temp==NULL){
          f->eax = -1;
        }
        else{
          file_sema_down();
          f->eax = file_read(temp,*(stack_pointer+2),*(stack_pointer+3));
          file_sema_up();
        }
      }

      break;

    case SYS_WRITE:
      //printf ("write file!\n");
      check_address(stack_pointer+1);//fd
      check_address(stack_pointer+2);//buffer
      check_address(*(stack_pointer+2));
      check_address(stack_pointer+3);//size
      //printf("%d\n",*(stack_pointer+1));
      //printf("%d\n",*(stack_pointer+3));
      if(*(stack_pointer+1)==1){
        putbuf(*(stack_pointer+2),*(stack_pointer+3));
        f->eax = *(stack_pointer+3);
      }
      else{
        struct file* temp2 = fd2fp(*(stack_pointer+1),2);
        if(temp2==NULL){
          //printf("no find\n");
          f->eax = -1;
        }
        else{
          file_sema_down();
          f->eax = file_write(temp2,*(stack_pointer+2),*(stack_pointer+3));
          file_sema_up();
          //printf("a%d\n",f->eax);
        }
      }
      //printf ("write finish!\n");
      break;

    case SYS_OPEN:
      check_address((stack_pointer+1));
      check_address(*(stack_pointer+1));
      file_sema_down();
      struct file* target_file = filesys_open(*(stack_pointer+1));
      file_sema_up();
      if(target_file != NULL){
        struct file_search* temp1 = malloc(sizeof(struct file_search));
        temp1->fp = target_file;
        temp1->fd = thread_current()->increase_file_id_generate;
        thread_current()->increase_file_id_generate++;
        list_push_back(&thread_current()->files,&temp1->elem);

        struct inode *inode= file_get_inode(target_file);
        if(inode != NULL && in_dir(inode)){
          temp1->dir = dir_open(inode_reopen(inode));
        }
        else{
          temp1->dir = NULL;
        }
        f->eax = temp1->fd;
      }
      else{
        f->eax = -1;
      }
      break;

    case SYS_CLOSE:
      check_address(stack_pointer+1);//fd
      //to do
      for(struct list_elem *e = list_begin(&thread_current()->files);e != list_end(&thread_current()->files);e = list_next(e)){
        if(list_entry(e,struct file_search,elem)->fd == *(stack_pointer+1)){
          file_sema_down();
          file_close(list_entry(e,struct file_search,elem)->fp);
          if(list_entry(e,struct file_search,elem)->dir){
            dir_close(list_entry(e,struct file_search,elem)->dir);
          }
          file_sema_up();
          list_remove(e);
        }
      }
      break;

    case SYS_FILESIZE:
      check_address(stack_pointer+1);//get fd, convert fd to file pointer.
      
      struct file * temp3 = fd2fp(*(stack_pointer+1),2);
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
      check_address(stack_pointer+1);//fd
      check_address(stack_pointer+2);//position
      file_sema_down();
      file_seek(fd2fp(*(stack_pointer+1),2),*(stack_pointer+2));//to do
      file_sema_up();
      break;

    case SYS_TELL:
      check_address(stack_pointer+1);
      struct file * temp4 = fd2fp(*(stack_pointer+1),2);
      if(temp4 != NULL){
        file_sema_down();
        f->eax = file_tell(temp4);//to do
        file_sema_up();
      }
      else{
        f->eax = -1;
      }
      break;

    case SYS_CHDIR:
      check_address(stack_pointer+1);
      file_sema_down();
      f->eax = sys_chdir(*(stack_pointer+1));
      file_sema_up();
      break;
    
    case SYS_MKDIR:
      check_address(stack_pointer+1);
      file_sema_down();
      f->eax = filesys_create(*(stack_pointer+1),0,true);
      file_sema_up();
      break;

    case SYS_READDIR:
      check_address(stack_pointer+1);
      check_address(stack_pointer+2);
      file_sema_down();
      f->eax = sys_readir(*(stack_pointer+1),*(stack_pointer+2));
      file_sema_up();
      break;

    case SYS_ISDIR:
      check_address(stack_pointer+1);
      file_sema_down();
      f->eax = in_dir(file_get_inode(fd2fp(*(stack_pointer+1),0)));
      file_sema_up();
      break;

    case SYS_INUMBER:
      check_address(stack_pointer+1);
      file_sema_down();
      f->eax = (int)inode_get_inumber(file_get_inode(fd2fp(*(stack_pointer+1),0)));
      file_sema_up();
      break;

    default:
      error_exit();
      break;
  }
  //printf ("system call!\n");
  //thread_exit ();//........big bug here!!!!!!!!!!!!!!!!!!
}