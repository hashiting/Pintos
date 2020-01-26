#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/synch.h"

#define MAX_FILESYS_CACHE_SIZE 64

struct filesys_cache_entry{
    bool free;
    block_sector_t disk_sector;
    uint8_t data[BLOCK_SECTOR_SIZE];
    bool dirty;
    bool accessed;
};

struct filesys_cache_entry cache[MAX_FILESYS_CACHE_SIZE];
struct lock filesys_cache_lock;

void filesys_cache_init();
void filesys_cache_close();

void filesys_cache_read(block_sector_t sector, void *uaddr);//read SECTOR_SIZE bytes
void filesys_cache_write(block_sector_t sector, void *uaddr);//write SECTOR_SIZE bytes

#endif //FILESYS_CACHE_H
