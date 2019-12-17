#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "lib/user/syscall.h"

static mapid_t map_increase_id = 1;
enum page_status{
    FRAME,
    SWAP,
    NEW,
    FILE
};

struct page_entry{
    void *user_adress;
    void *kernel_adress;
    enum page_status status;

    bool dirty;
    int swap;
    struct hash_elem helem;
    struct file* file;
    int read_bytes;
    int zero_bytes;
};

struct map_info
{
    struct list_elem elem;
    struct file* file;
    mapid_t id;
    void* user_adress;
    int size;
};

struct hash* page_init();//by thread
unsigned p_hash_hash_func(const struct hash_elem *e , void *aux);
bool p_hash_less_func(const struct hash_elem *a,const struct hash_elem *b,void *aux);

struct page_entry* set_page_entry(void* user_adress, void* kernel_address,enum page_status s,int swap_index);
struct page_entry* get_page_entry(struct hash *h,void *user_adress);

bool Install_page_in_frame(struct hash* h,void *user_adress, void *kernel_adress,int swap_index);
bool Install_new_page(struct hash* h,void *user_adress);
void Set_page_swap(struct hash *h, void* user_adress,int swap);

bool load_page(struct hash* h,uint32_t *pagedir, void *user_adress);

mapid_t mmap(int fd, void *user_adress);
bool unmmap(mapid_t id);
struct map_info* mmap_entity(mapid_t id,struct file* file,void* user_adress,int file_size);
mapid_t mmap_insert(struct file* file,void* user_adress,int file_size);
#endif