#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

void swap_init(){
    swap_block = block_get_role(BLOCK_SWAP);
    swap_bitmap = bitmap_create(block_size(swap_block)/NUM_SECTOR);
    bitmap_set_all(swap_bitmap,true);//true means slot available for swap
}

int swap_out(void* address){
    int temp = bitmap_scan(swap_bitmap,0,1,true);//get one for use
    if(temp == BITMAP_ERROR)
      PANIC("swap device is full\n");
    for(int i = 0;i < NUM_SECTOR;i++){//write address to sector with bias
        block_write(swap_block,temp*NUM_SECTOR + i,BLOCK_SECTOR_SIZE*i + address);
    }
    bitmap_set(swap_bitmap,temp,false);//set false
    return temp;
}

void swap_in(void *address,int index){
    if(bitmap_test(swap_bitmap, index))
        PANIC("try to swap back an empty swap slot\n");
    for(int i = 0;i < NUM_SECTOR;i++){//put sector to user address
        block_read(swap_block,index*NUM_SECTOR + i,BLOCK_SECTOR_SIZE*i + address);
    }
    bitmap_set(swap_bitmap,index,true);
}

void swap_remove(int index)
{
  if(bitmap_test(swap_bitmap, index))
    PANIC("try to swap back an empty swap slot\n");
  bitmap_set(swap_bitmap,index,true);
}