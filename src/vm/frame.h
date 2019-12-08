#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"

struct hash frame_table; //psysical add -> fram
struct lock frame_lock;
struct list frame_list;

struct frame_entry{
    struct hash_elem helem;
    struct list_elem lelem;//element

    void *user_adress;
    void *kernel_adress;
    int evict_num;//for replacement
    struct thread *t;
};

void frame_init();
struct frame_entry* frame_entity_init(void* user_adress,void *kernel_address,int evict);
struct frame_entry* frame_allo(enum palloc_flags flags,void* user_adress);
unsigned hash_hash_func(const struct hash_elem *e , void *aux);
bool hash_less_func(const struct hash_elem *a,const struct hash_elem *b,void *aux);
void frame_remove(struct frame_entry* entity);
void frame_free(struct frame_entry* entity);
struct frame_entry* frame_clock(uint32_t *pagedir);
#endif