#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

void swap_init(){
    swap_block = block_get_role(BLOCK_SWAP);
    swap_bitmap = bitmap_create(block_size(swap_block)/NUM_SECTOR);
    bitmap_set_all(swap_bitmap,true);
}

int swap_out(void* adress){
    int temp = bitmap_scan(swap_bitmap,0,1,true);//get one for use
    for(int i = 0;i < NUM_SECTOR;i++){//write address to sector with bias
        block_write(swap_block,temp*NUM_SECTOR + i,BLOCK_SECTOR_SIZE*i + adress);
    }
    bitmap_set(swap_bitmap,temp,false);//set false
    return temp;
}

void swap_in(void *adress,int index){
    for(int i = 0;i < NUM_SECTOR;i++){//put sector to user address
        block_read(swap_block,index*NUM_SECTOR + i,BLOCK_SECTOR_SIZE*i + adress);
    }
    bitmap_set(swap_bitmap,index,true);
}