#ifndef VM_PAGE_H
#define VM_PAGE_H


enum page_status{
    FRAME,
    SWAP,
    EXEU,
    OTHER
};

struct hash Supplemental_page_table;



#endif