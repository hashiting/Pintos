#include <lib/debug.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"

void filesys_cache_init(){
  lock_init(&filesys_cache_lock);

  int i ;
  for(i = 0; i < MAX_FILESYS_CACHE_SIZE; i++){
    cache[i].free = true;
  }
}

void filesys_cache_flush(struct filesys_cache_entry *entry){
  ASSERT(lock_held_by_current_thread(&filesys_cache_lock));
  ASSERT(entry != NULL && entry->free == false);

  if(entry->dirty){
    block_write(fs_device, entry->disk_sector, entry->data);
    entry->dirty = false;
  }
}

void filesys_cache_close(){
  //flush all cache in filesys_done()
  lock_acquire(&filesys_cache_lock);

  int i;
  for(i = 0; i < MAX_FILESYS_CACHE_SIZE; i++){
    if(cache[i].free)
      continue;
    filesys_cache_flush(&cache[i]);
  }

  lock_release(&filesys_cache_lock);
}

struct filesys_cache_entry* filesys_cache_lookup(block_sector_t sector){
  int i;
  for(i = 0; i < MAX_FILESYS_CACHE_SIZE; i++){
    if(cache[i].free)
      continue;
    if(cache[i].disk_sector == sector)
      return &(cache[i]);//cache hit
  }
  return NULL;//cache miss
}

struct filesys_cache_entry* filesys_cache_evict(){
  ASSERT(lock_held_by_current_thread(&filesys_cache_lock));

  //clock algorithm
  int i, j;
  for(i = 0;i < 2;i++){
    for(j = 0; j < MAX_FILESYS_CACHE_SIZE; j++){
      if(cache[j].free)
        return &(cache[j]);
      if(cache[j].accessed){
        cache[j].accessed = false;
      }else{
        break;//evict this cache entry
      }
    }
  }

  struct filesys_cache_entry* to_evict = &(cache[j]);
  if(to_evict->dirty)
    filesys_cache_flush(to_evict);
  to_evict->free = true;
  return to_evict;
}

void filesys_cache_read(block_sector_t sector, void *uaddr){
  lock_acquire(&filesys_cache_lock);

  struct filesys_cache_entry* entry = filesys_cache_lookup(sector);
  if(entry == NULL){
    //if not in cache
    entry = filesys_cache_evict();
    ASSERT(entry != NULL);
    entry->free = false;
    entry->dirty = false;
    entry->disk_sector = sector;
    block_read(fs_device, sector, entry->data);
  }
  //now it must be in cache
  entry->accessed = true;
  //copy data from cache to uaddr
  memcpy(uaddr, entry->data, BLOCK_SECTOR_SIZE);

  lock_release(&filesys_cache_lock);
}

void filesys_cache_write(block_sector_t sector, void *uaddr){
  lock_acquire(&filesys_cache_lock);
  struct filesys_cache_entry* entry = filesys_cache_lookup(sector);
  if(entry == NULL){
    //if not in cache
    entry = filesys_cache_evict();
    ASSERT(entry != NULL);
    entry->free = false;
    entry->disk_sector = sector;
    block_read(fs_device, sector, entry->data);
  }
  //now it must be in cache
  entry->dirty = true;
  entry->accessed = true;
  //copy data from uaddr to cache
  memcpy(entry->data, uaddr, BLOCK_SECTOR_SIZE);

  lock_release(&filesys_cache_lock);
}