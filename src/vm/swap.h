#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"
#include <bitmap.h>
#include "threads/vaddr.h"

#define NUM_SECTOR PGSIZE / BLOCK_SECTOR_SIZE

struct block *swap_block;
struct bitmap *swap_bitmap;

void swap_init();
int swap_out(void *adress);
void swap_in(void *adress, int index);


#endif