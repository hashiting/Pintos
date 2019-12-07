#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

unsigned hash_hash_func(const struct hash_elem *e, void *aux){
    return hash_bytes(&hash_entry(e, struct frame_entry, helem)->kernel_adress, sizeof(hash_entry(e, struct frame_entry, helem)->kernel_adress));//set psy adress as key
}

bool hash_less_func(const struct hash_elem *a,const struct hash_elem *b,void *aux){
    return hash_entry(a, struct frame_entry, helem)->kernel_adress < hash_entry(b, struct frame_entry, helem)->kernel_adress;
}

void frame_init(){
    lock_init(&frame_lock);
    list_init(&frame_list);
    hash_init(&frame_table,hash_hash_func,hash_less_func,NULL);
}

uint8_t* frame_allo(enum palloc_flags flags,void* user_adress){
    lock_acquire(&frame_lock);

    lock_release(&frame_lock);
}