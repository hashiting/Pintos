#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include <hash.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "lib/string.h"

struct hash* page_table_init(){
    struct hash* Page_table = (struct hash*) malloc(sizeof(struct hash));
    hash_init(Page_table,p_hash_hash_func,p_hash_less_func,NULL);
    return Page_table;
}

void hash_destruct_func(struct hash_elem *e, void *aux){
  struct page_entry *entry = hash_entry(e, struct page_entry, helem);
  if(entry->kernel_address != NULL){
    struct frame_entry *frame_entry = kad2fe(entry->kernel_address);
    frame_remove(frame_entry);
  }
  free(entry);
}

void page_table_free(struct hash* page_table){
  ASSERT(page_table != NULL);
  hash_destroy(page_table, hash_destruct_func);
  free(page_table);
}

unsigned p_hash_hash_func(const struct hash_elem *e , void *aux){
    return hash_bytes(&hash_entry(e, struct page_entry, helem)->user_address, sizeof(hash_entry(e, struct page_entry, helem)->user_address));//set user address as key
}

bool p_hash_less_func(const struct hash_elem *a,const struct hash_elem *b,void *aux){
    return hash_entry(a, struct page_entry, helem)->user_address < hash_entry(b, struct page_entry, helem)->user_address;
}

struct page_entry* set_page_entry(void* user_address, void* kernel_address,enum page_status s,int swap){
    struct page_entry* temp = (struct page_entry*)malloc(sizeof(struct page_entry));
    temp->user_address = user_address;
    temp->kernel_address = kernel_address;
    temp->status = s;
    temp->swap = swap;
    return temp;
}

struct page_entry* get_page_entry(struct hash *h,void *user_address){
    struct page_entry* temp = (struct page_entry*)malloc(sizeof(struct page_entry));
    temp->user_address = user_address;
    struct hash_elem *helem = hash_find(h,&temp->helem);
    if(helem != NULL){
        free(temp);
        return hash_entry(helem,struct page_entry,helem);
    }
    return NULL;    
}

bool Install_page_in_frame(struct hash *h, void *user_address, void *kernel_address)
{
    struct page_entry* temp = set_page_entry(user_address,kernel_address,FRAME,-100);
    temp->dirty = false;
    struct hash_elem *helem = hash_insert(h,&temp->helem);
    if(helem == NULL){
        return true;
    }else{
      free(temp);
      return false;
    }
}

bool Install_new_page(struct hash* h,void *user_address){
    struct page_entry* temp = set_page_entry(user_address,NULL,NEW,-100);
    temp->dirty = false;
    struct hash_elem *helem = hash_insert(h,&temp->helem);
    if(helem == NULL){
        return true;
    }else{
      free(temp);
      return false;
    }
}

void Set_page_swap(struct hash *h, void* user_address,int swap){
    struct page_entry* temp = get_page_entry(h,user_address);
    set_page_entry(user_address,NULL,SWAP,swap);
}

bool Install_page_in_file(struct hash *h, void *user_address, struct file *file,
  off_t file_offset, int read_bytes, int zero_bytes, bool writable){
  struct page_entry* entry = (struct page_entry*)malloc(sizeof(struct page_entry));
  entry->user_address = user_address;
  entry->kernel_address = NULL;
  entry->status = FILE;
  entry->dirty = false;
  entry->file = file;
  entry->file_offset = file_offset;
  entry->read_bytes = read_bytes;
  entry->zero_bytes = zero_bytes;
  entry->writable = writable;

  struct hash_elem *e = hash_insert(h, &entry->helem);
  if(e == NULL)
    return true;
  PANIC("already an entry in page_table for this file\n");
}

bool load_page_from_file(struct page_entry *entry, void *kpage){
  file_seek(entry->file, entry->file_offset);
  off_t bytes_read = file_read(entry->file, kpage, entry->read_bytes);//read some file content into kpage
  if(bytes_read != entry->read_bytes)
    return false;
  ASSERT(entry->read_bytes + entry->zero_bytes == PGSIZE);
  memset(kpage + bytes_read, 0, entry->zero_bytes);//fill the rest of the kpage with 0
  return true;
}

bool load_page(struct hash* h,uint32_t *pagedir, void *user_address){
    struct page_entry* pe = get_page_entry(h,user_address);
    if(pe == NULL){
        return false;
    }
    if(pe->status == FRAME){
        return true;
    }

    void* frame = frame_allocate(PAL_USER, user_address)->kernel_address;
    bool writable = true;
    if(pe->status == NEW){
        memset(frame,0,PGSIZE);//set zero
    }
    else if(pe->status == SWAP){
        swap_in(frame,pe->swap);
    }
    else if(pe->status == FILE){//file
        bool success = load_page_from_file(pe, frame);
        if(!success){
          frame_free(frame);
          return false;
        }
        writable = pe->writable;
    }else{
      PANIC("no such page_table entry status\n");
    }
    
    //to do//v -> p
    struct frame_entry* temp = kad2fe(frame);
    bool success = pagedir_set_page(pagedir,user_address,frame,writable);//can be modified by file
    if(success){
        pe->kernel_address = frame;
        pe->status = FRAME;
        frame_unpin(temp);
        pagedir_set_dirty(pagedir,frame,false);//same
        return true;
    }
    else{
        frame_free(temp);
        return false;
    }
}

void
page_set_dirty (struct hash* h, void *user_address, bool dirty) 
{
  struct page_entry* temp = get_page_entry(h,user_address);
  if (temp != NULL) {
    temp->dirty = dirty;
  }
    PANIC("error");
}
struct map_info* mmap_entity(mapid_t id,struct file* file,void* user_address,int file_size){
    struct map_info *temp = (struct map_info*) malloc(sizeof(struct map_info));
    temp->id = id;
    temp->file = file;
    temp->user_address = user_address;
    temp->size = file_size;
    return temp;
}

struct map_info* get_mmap_entity(struct list *mmaps, mapid_t id){
  if(list_empty(mmaps))
    return NULL;
  struct list_elem *e;
  for(e = list_begin(mmaps); e!= list_end(mmaps); e = list_next(e)){
    struct map_info *map_info = list_entry(e, struct map_info, elem);
    if(map_info->id == id)
      return map_info;
  }
  return NULL;
}

mapid_t mmap_insert(struct file* file,void* user_address,int file_size){
    struct map_info* temp = mmap_entity(map_increase_id,file,user_address,file_size);
    map_increase_id++;//unique
    list_push_back (&thread_current()->mmaps, &temp->elem);
    return map_increase_id-1;
}

mapid_t mmap(int fd, void *user_address){
  if(fd <= 1){
    return -1;
  }
  struct thread *t = thread_current();

  file_sema_down();
  struct file* temp_file = file_reopen(fd2fp(fd));
  if(temp_file != NULL){
    off_t len = file_length(temp_file);//len = 0?
    for(int i = 0;i < len;i += PGSIZE){
      void *temp = user_address + i;
      if(get_page_entry(thread_current()->page_table,temp)!=NULL){
          file_sema_up();
          return -1;
      }
    }
//check done, now map
    for(int i = 0;i < len;i += PGSIZE){
      void *temp_addr = user_address + i;
      int read_bytes = PGSIZE;
      if(i + PGSIZE >= len){
        read_bytes = len - i;
      }
      int zero_bytes = PGSIZE - read_bytes;//rest
      Install_page_in_file(t->page_table, temp_addr, temp_file, i, read_bytes, zero_bytes, true);
    }
//insert into list
    mapid_t result = mmap_insert(temp_file,user_address,len);
    file_sema_up();
    return result;
  }
  else{
    file_sema_up();
    return -1;
  }
}

bool unmmap(mapid_t id)
{
  struct thread *t = thread_current();
  struct map_info *map_info = get_mmap_entity(&t->mmaps, id);
  if(map_info == NULL)
    return false;
  file_sema_down();
  off_t ofs = 0;
  int file_size = map_info->size;
  //ummap each page of file
  for(ofs = 0; ofs < file_size; ofs = ofs + PGSIZE){
    file_seek(map_info->file, ofs);
    int bytes = PGSIZE;
    if(file_size - ofs < PGSIZE)
      bytes = file_size - ofs;
    void *upage = map_info->user_address;
    struct page_entry *page_entry = get_page_entry(t->page_table, upage);
    ASSERT(page_entry != NULL)
    struct frame_entry *frame_entry = kad2fe(page_entry->kernel_address);
    bool dirty;
    switch (page_entry->status){
      case FRAME:
        frame_pin(frame_entry);
        //dirty: write back to file
        dirty = page_entry->dirty
          || pagedir_is_dirty(t->pagedir, page_entry->user_address)
          || pagedir_is_dirty(t->pagedir, page_entry->kernel_address);
        if(dirty){
          file_write_at(map_info->file, page_entry->user_address, bytes, ofs);
        }
        //remove from frame table
        frame_free(frame_entry);
        //remove from pagedir
        pagedir_clear_page(t->pagedir, page_entry->user_address);
        break;
      case SWAP:
        //dirty: swap in, write back to file
        dirty = page_entry->dirty
          || pagedir_is_dirty(t->pagedir, page_entry->user_address);
        if(dirty){
          void *temp_addr = palloc_get_page(0);
          swap_in(temp_addr, page_entry->swap);
          file_write_at(map_info->file, temp_addr, PGSIZE, ofs);
          palloc_free_page(temp_addr);
        }
        //not dirty: remove from swap bitmap
        swap_remove(page_entry->swap);
        break;
      case FILE://TODO
        break;
      default:
        PANIC("NEW pages shouldn't be unmapped\n");
    }
    hash_delete(t->page_table, &page_entry->helem);
  }
  list_remove(& map_info->elem);
  file_close(map_info->file);
  free(map_info);
  file_sema_up();
  return true;
}