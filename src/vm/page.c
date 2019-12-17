#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include <hash.h>
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

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

struct page_entry* set_page_entry(void* user_adress, void* kernel_address,enum page_status s,int swap){
    struct page_entry* temp = (struct page_entry*)malloc(sizeof(struct page_entry));
    temp->user_adress = user_adress;
    temp->kernel_adress = kernel_address;
    temp->status = s;
    temp->swap = swap;
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

bool Install_page_in_frame(struct hash* h,void *user_adress, void *kernel_adress,int swap){
    struct page_entry* temp = set_page_entry(user_adress,kernel_adress,FRAME,-100);
    temp->dirty = false;
    struct hash_elem *helem = hash_insert(h,&temp->helem);
    if(helem == NULL){
        return true;
    }
    return false;
}

bool Install_new_page(struct hash* h,void *user_adress){
    struct page_entry* temp = set_page_entry(user_adress,NULL,NEW,-100);
    temp->dirty = false;
    struct hash_elem *helem = hash_insert(h,&temp->helem);
    if(helem == NULL){
        return true;
    }
    return false;
}

void Set_page_swap(struct hash *h, void* user_adress,int swap){
    struct page_entry* temp = get_page_entry(h,user_adress);
    set_page_entry(user_adress,NULL,SWAP,swap);
}

bool Install_page_in_file(){
    //to do
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
        memset(frame,0,PGSIZE);//set zero
    }
    else if(pe->status == SWAP){
        swap_in(frame,pe->swap);
    }
    else{//file
        //to do 
    }
    
    //to do//v -> p
    struct frame_entry* temp = kad2fe(frame);
    bool success = pagedir_set_page(pagedir,user_adress,frame,true);
    if(success){
        pe->kernel_adress = frame;
        pe->status = FRAME;
        frame_unpin(temp);
        pagedir_set_dirty(pagedir,frame,false);
        return true;
    }
    else{
        frame_free(temp);
        return false;
    }
}

void
page_set_dirty (struct hash* h, void *user_adress, bool dirty) 
{
  struct page_entry* temp = get_page_entry(h,user_adress);
  if (temp != NULL) {
    temp->dirty = dirty;
  }
    PANIC("error");
}


