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
    //INVALID,
    FRAME,
    SWAP,
    NEW,
    FILE
};

struct map_info
{
    struct list_elem elem;
    struct file* file;
    mapid_t id;
    void* user_address;
    int size;
};

struct page_entry{
    void *user_address;
    void *kernel_address;
    enum page_status status;

    bool dirty;
    int swap;
    struct hash_elem helem;
    struct file* file;
    off_t file_offset;
    int read_bytes;
    int zero_bytes;
    bool writable;
};

struct hash* page_table_init();//by thread
void page_table_free(struct hash* page_table);
unsigned p_hash_hash_func(const struct hash_elem *e , void *aux);
bool p_hash_less_func(const struct hash_elem *a,const struct hash_elem *b,void *aux);

struct page_entry* set_page_entry(void* user_address, void* kernel_address,enum page_status s,int swap_index);
struct page_entry* get_page_entry(struct hash *h,void *user_address);

bool Install_page_in_frame(struct hash *h, void *user_address, void *kernel_address);
bool Install_new_page(struct hash* h,void *user_address);
void Set_page_swap(struct hash *h, void* user_address,int swap);
bool Install_page_in_file(struct hash *h, void *user_address, struct file *file,
                          off_t file_offset, int read_bytes, int zero_bytes, bool writable);

bool load_page(struct hash* h,uint32_t *pagedir, void *user_address);
void page_pin(struct hash *h, void *user_address);
void page_unpin(struct hash *h, void *user_address);
void buffer_load_and_pin(void *buffer, size_t size);
void buffer_unpin(void *buffer, size_t size);

void page_set_dirty (struct hash* h, void *user_address, bool dirty);

mapid_t sys_mmap(int fd, void *user_address);
bool sys_unmmap(mapid_t id);
struct map_info* mmap_entity(mapid_t id,struct file* file,void* user_address,int file_size);
mapid_t mmap_insert(struct file* file,void* user_address,int file_size);
#endif