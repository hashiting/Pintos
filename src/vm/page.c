#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include <hash.h>
#include "threads/vaddr.h"

struct hash* page_init(){
    struct hash* Page_table = (struct hash*) malloc(sizeof(struct hash));
    hash_init(Page_table,p_hash_hash_func,p_hash_less_func,NULL);
    return Page_table;
}

unsigned p_hash_hash_func(const struct hash_elem *e , void *aux){
    return hash_bytes(&hash_entry(e, struct page_entry, helem)->user_adress, sizeof(hash_entry(e, struct page_entry, helem)->user_adress));//set user adress as key
}

bool p_hash_less_func(const struct hash_elem *a,const struct hash_elem *b,void *aux){
    return hash_entry(a, struct page_entry, helem)->user_adress < hash_entry(b, struct page_entry, helem)->user_adress;
}

struct page_entry* set_page_entry(void* user_adress, void* kernel_address,enum page_status s,bool dirty,int swap_index){
    struct page_entry* temp = (struct page_entry*)malloc(sizeof(struct page_entry));
    temp->user_adress = user_adress;
    temp->kernel_adress = kernel_address;
    temp->status = s;
    temp->dirty = dirty;
    temp->swap_index = swap_index;
    return temp;
}

struct page_entry* get_page_entry(struct hash *h,void *user_adress){
    struct page_entry* temp = (struct page_entry*)malloc(sizeof(struct page_entry));
    temp->user_adress = user_adress;
    struct hash_elem *helem = hash_find(h,&temp->helem);
    if(helem != NULL){
        free(temp);
        return hash_entry(helem,struct page_entry,helem);
    }
    return NULL;    
}

bool Install_page_in_frame(struct hash* h,void *user_adress, void *kernel_adress,int swap_index){
    struct page_entry* temp = set_page_entry(user_adress,kernel_adress,FRAME,false,-100);
    struct hash_elem *helem = hash_insert(h,&temp->helem);
    if(helem == NULL){
        return true;
    }
    return false;
}

bool Install_new_page(struct hash* h,void *user_adress){
    struct page_entry* temp = set_page_entry(user_adress,NULL,NEW,false,-100);
    struct hash_elem *helem = hash_insert(h,&temp->helem);
    if(helem == NULL){
        return true;
    }
    return false;
}

void Set_page_swap(){

}

bool Install_page_in_file(){

}

bool load_page(struct hash* h,uint32_t *pagedir, void *user_adress){
    struct page_entry* pe = get_page_entry(h,user_adress);
    if(pe == NULL){
        return false;
    }
    if(pe->status == FRAME){
        return true;
    }

    void* frame = frame_allo(PAL_USER,user_adress)->kernel_adress;
    if(pe->status == NEW){
        memset(frame,0,PGSIZE);
    }
    else if(pe->status == SWAP){
        //to do
    }
    else{//file
        //to do
    }
    
    //to do
}

