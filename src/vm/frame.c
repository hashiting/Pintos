#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"

unsigned f_hash_hash_func(const struct hash_elem *e, void *aux){
    return hash_bytes(&hash_entry(e, struct frame_entry, helem)->kernel_adress, sizeof(hash_entry(e, struct frame_entry, helem)->kernel_adress));//set psy adress as key
}

bool f_hash_less_func(const struct hash_elem *a,const struct hash_elem *b,void *aux){
    return hash_entry(a, struct frame_entry, helem)->kernel_adress < hash_entry(b, struct frame_entry, helem)->kernel_adress;
}

void frame_init(){
    lock_init(&frame_lock);
    list_init(&frame_list);
    hash_init(&frame_table,f_hash_hash_func,f_hash_less_func,NULL);
}

//call this function in init.c as frame is global
struct frame_entry* frame_entity_init(void* user_adress,void *kernel_address,int pin){
    struct frame_entry* temp = malloc(sizeof(struct frame_entry));
    temp->t = thread_current();
    temp->user_adress = user_adress;
    temp->kernel_adress = kernel_address;
    temp->pin = pin;
    return temp;
}

struct frame_entry* frame_allo(enum palloc_flags flags,void* user_adress){
    lock_acquire(&frame_lock);
    void *kernel_adress = palloc_get_page(PAL_USER | flags);//get page
    if(kernel_adress != NULL){
        struct frame_entry* entity = frame_entity_init(user_adress,kernel_adress,0);
        hash_insert(&frame_table, &entity->helem);
        list_push_back(&frame_list, &entity->lelem);
        lock_release(&frame_lock);
        return entity;
    }
    else{
        //to do // swap someone
        struct frame_entry *evict = frame_clock(thread_current()->pagedir);
        pagedir_clear_page(evict->t->pagedir, evict->user_adress);
        //to do
        lock_release(&frame_lock);
        return NULL;
    }
}

//run kad2fe before this two function
void frame_remove(struct frame_entry* entity){
    hash_delete(&frame_table,&entity->helem);
    list_remove(&entity->lelem);
}
void frame_free(struct frame_entry* entity){
    frame_remove(entity);
    palloc_free_page(entity->kernel_adress);
    free(entity);
}

//swap using clock//run two time
struct frame_entry* frame_clock(uint32_t *pagedir){
    for(int i = 0;i < 2;i++){
        for(struct frame_entry* temp = list_begin (&frame_list);temp != list_end(&frame_list);temp = list_next(temp)){
            if(temp->pin != 0){
                if(!pagedir_is_accessed(pagedir, temp->kernel_adress)){
                    return temp;
                }
                else{
                    pagedir_set_accessed(pagedir, temp->kernel_adress, false);
                    //return temp;
                }
            }
        }
    }
    PANIC("no memory");
    return NULL; 
}

//run kad2fe before this two function
void frame_pin(struct frame_entry* entity){
    lock_acquire(&frame_lock);
    entity->pin++;
    lock_release(&frame_lock);
}
void frame_unpin(struct frame_entry* entity){
    lock_acquire(&frame_lock);
    entity->pin = 0;
    lock_release(&frame_lock);
}

struct frame_entry* kad2fe(void *kernel_adress){
    struct frame_entry* temp = malloc(sizeof(struct frame_entry));
    temp->kernel_adress = kernel_adress;
    struct hash_elem *h = hash_find (&frame_table, &temp->helem);
    if(h != NULL){
        return hash_entry(h,struct frame_entry,helem);
    }
    return NULL;
}