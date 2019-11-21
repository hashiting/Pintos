/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#define _BUFFER_SIZE 4

//char* Source = "Hello world"; 


char Source[11] = "Hello world";
int position = 0;
char Buffer[_BUFFER_SIZE];
int producer_address = 0;
int consumer_address = 0;
int current_elem_in_buffer = 0;

void producer_add_address(){
    position++;
    producer_address++;
    current_elem_in_buffer++;
    producer_address = producer_address%_BUFFER_SIZE;
    return;
}

void consumer_add_address(){
    consumer_address++;
    current_elem_in_buffer--;
    consumer_address = consumer_address%_BUFFER_SIZE;
    return;
}
struct condition cond_empty;
struct condition cond_fill;
struct lock mutex_lock;

void producer()
{
    while(position < 11){
        lock_acquire(&mutex_lock);
        while(current_elem_in_buffer >= _BUFFER_SIZE){
            cond_wait(&cond_fill,&mutex_lock);
        }
        Buffer[producer_address] = Source[position];
        producer_add_address();
        cond_signal(&cond_empty,&mutex_lock);
        lock_release(&mutex_lock);
    }
    return;
}

void consumer()
{
    while(1){
        lock_acquire(&mutex_lock);
        while(current_elem_in_buffer <= 0){
            cond_wait(&cond_empty,&mutex_lock);
        }
        if(position >= 11 && current_elem_in_buffer == 0){
            break;
        }
        printf("%c",Buffer[consumer_address]);
        consumer_add_address();
        cond_signal(&cond_fill,&mutex_lock);
        lock_release(&mutex_lock);
    }
    return;
}

void producer_consumer(unsigned int num_producer, unsigned int num_consumer);


void test_producer_consumer(void)
{
    producer_consumer(7, 2);
    /*producer_consumer(1, 0);
    producer_consumer(0, 1);
    producer_consumer(1, 1);
    producer_consumer(3, 1);
    producer_consumer(1, 3);
    producer_consumer(4, 4);
    producer_consumer(7, 2);
    producer_consumer(2, 7);*/
    //producer_consumer(6, 6);
    pass();
}


void producer_consumer(UNUSED unsigned int num_producer, UNUSED unsigned int num_consumer)
{
    //msg("NOT IMPLEMENTED");
    /* FIXME implement */
    //Queue* instance = Cir_Queue_init();
    cond_init(&cond_empty);
    cond_init(&cond_fill);
    lock_init(&mutex_lock);
    for(unsigned int i = 0;i < num_producer;i++){
       thread_create("producer",1,producer,NULL);
    }

    for(unsigned int i = 0;i < num_consumer;i++){
       thread_create("consumer",1,consumer,NULL);
    }
    return;

}


