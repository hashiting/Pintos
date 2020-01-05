#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"

struct hash frame_table; //psysical add -> fram//global
struct lock frame_lock;
struct list frame_list;//for clock algorithm

struct frame_entry{
    struct hash_elem helem;
    struct list_elem lelem;//element

    void *user_address;
    void *kernel_address;
    bool pinned;//if true: can't be replace, because not loaded/installed.
    struct thread *t;
};

void frame_table_init();
struct frame_entry* frame_entity_init(void* user_address, void *kernel_address, bool pinned);
struct frame_entry* frame_allocate(enum palloc_flags flags, void* user_address);
struct frame_entry* frame_clock(uint32_t *pagedir);

unsigned f_hash_hash_func(const struct hash_elem *e , void *aux);
bool f_hash_less_func(const struct hash_elem *a,const struct hash_elem *b,void *aux);

void frame_remove(struct frame_entry* entity);
void frame_free(struct frame_entry* entity);

void frame_pin(struct frame_entry* entity);
void frame_unpin(struct frame_entry* entity);

//kernel address to frame entry entity
struct frame_entry* kad2fe(void *kernel_address);
#endif