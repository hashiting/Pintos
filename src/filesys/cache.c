#include "filesys/cache.h"
#include "filesys/filesys.h"

void filesys_cache_init(){
  lock_init(&filesys_cache_lock);

  int i ;
  for(i = 0; i < MAX_FILESYS_CACHE_SIZE; i++){
    cache[i].free = true;
  }
}

void filesys_cache_close(){
  //flush all cache in filesys_done()
}

void filesys_cache_read(block_sector_t sector, void *uaddr){
  //if in cache, read directly from cache

  //if not in cache
  block_read(fs_device, sector, uaddr);

  //copy data from cache to uaddr
}

void filesys_cache_write(block_sector_t sector, void *uaddr){
  //if in cache, write directly to cache

  //if not in cache
  block_write(fs_device, sector, uaddr);

  //copy data from uaddr to cache
}