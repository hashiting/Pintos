#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/file.h"

enum page_status{
    FRAME,
    SWAP,
    NOUSE,
    FILE
};

struct hash Page_table;

struct page_entry{
    void *user_adress;
    void *kernel_adress;
    enum page_status status;

    bool dirty;
    struct hash_elem helem;
    struct file* file;
    int read_bytes;
    int zero_bytes;
}



#endif