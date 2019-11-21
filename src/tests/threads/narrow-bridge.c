/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include "random.h"

void ArriveBridge(int direc, int prio);
void CrossBridge(int direc, int prio);
void ExitBridge(int direc, int prio);
void OneVehicle(void *arg);

struct car
{
    int direc;
    int prio;
};
int num_active_left = 0;
int num_active_right = 0;
int num_waiting_vehicles_left = 0;
int num_waiting_emergency_left = 0;
int num_waiting_vehicles_right = 0;
int num_waiting_emergency_right = 0;

struct semaphore OKToLeftForVehicle;
struct semaphore OKToRightForVehicle;
struct semaphore OKToLeftForEmergency;
struct semaphore OKToRightForEmergency;
struct semaphore mutex;

void narrow_bridge(unsigned int num_vehicles_left, unsigned int num_vehicles_right,
        unsigned int num_emergency_left, unsigned int num_emergency_right);


void test_narrow_bridge(void)
{
    /*narrow_bridge(0, 0, 0, 0);
    narrow_bridge(1, 0, 0, 0);
    narrow_bridge(0, 0, 0, 1);
    narrow_bridge(0, 4, 0, 0);
    narrow_bridge(0, 0, 4, 0);
    narrow_bridge(3, 3, 3, 3);
    narrow_bridge(4, 3, 4 ,3);
    narrow_bridge(7, 23, 17, 1);
    narrow_bridge(40, 30, 0, 0);
    narrow_bridge(30, 40, 0, 0);
    narrow_bridge(23, 23, 1, 11);
    narrow_bridge(22, 22, 10, 10);
    narrow_bridge(0, 0, 11, 12);
    narrow_bridge(0, 10, 0, 10);*/
    narrow_bridge(5, 0, 7, 4);
    pass();
}


void narrow_bridge(UNUSED unsigned int num_vehicles_left, UNUSED unsigned int num_vehicles_right,
        UNUSED unsigned int num_emergency_left, UNUSED unsigned int num_emergency_right)
{
    //msg("NOT IMPLEMENTED");
    /* FIXME implement */
    // initialize semaphores
    sema_init(&OKToLeftForEmergency, 0);
    sema_init(&OKToRightForEmergency, 0);
    sema_init(&OKToLeftForVehicle, 0);
    sema_init(&OKToRightForVehicle, 0);
    sema_init(&mutex, 1);

    // create threads for emergency vehicles to left
    for (int i = 0; i < num_emergency_left; i++)
    {
        struct car *Car = (struct car*)malloc(sizeof(struct car));
        Car->direc = 0;
        Car->prio = 1;
        thread_create("", PRI_DEFAULT, OneVehicle, (void*) Car);
    }

    // create threads for emergency vehicles to right
    for (int i = 0; i < num_emergency_right; i++)
    {
        struct car *Car = (struct car*)malloc(sizeof(struct car));
        Car->direc = 1;
        Car->prio = 1;
        thread_create("", PRI_DEFAULT, OneVehicle, (void*) Car);
    }

    // create threads for regular vehicles to left
    for (int i = 0; i < num_vehicles_left; i++)
    {
        struct car *Car = (struct car*)malloc(sizeof(struct car));
        Car->direc = 0;
        Car->prio = 0;
        thread_create("", PRI_DEFAULT, OneVehicle, (void*) Car);
    }

    // create threads for regular vehicles to right
    for (int i = 0; i < num_vehicles_right; i++)
    {
        struct car *Car = (struct car*)malloc(sizeof(struct car));
        Car->direc = 1;
        Car->prio = 0;
        thread_create("", PRI_DEFAULT, OneVehicle, (void*) Car);
    }

}

/* To block the thread until it is safe for the car to cross the bridge in the given direction
 */
void ArriveBridge(int direc, int prio) 
{
    sema_down(&mutex);
    // For vehicles whose direction is to left
    if (direc == 0 && num_active_right == 0 && num_active_left < 3)
    {
        if (prio == 1)
            sema_up(&OKToLeftForEmergency);
        else
            sema_up(&OKToLeftForVehicle);
        num_active_left++;
    }
    // For vehicles whose direction is to right
    else if (direc == 1 && num_active_left == 0 && num_active_right < 3)
    {
        if (prio == 1)
            sema_up(&OKToRightForEmergency);
        else
            sema_up(&OKToRightForVehicle);
        num_active_right++;
    }
    else if (direc == 0)
    {
        if (prio == 1)
            num_waiting_emergency_left++;
        else
            num_waiting_vehicles_left++;
        
    }
    else if (direc == 1)
    {
        if (prio == 1)
            num_waiting_emergency_right++;
        else
            num_waiting_vehicles_right++;
    }
    sema_up(&mutex);

    switch (direc * 2 + prio)
    {
        case 0:
            sema_down(&OKToLeftForVehicle);
            break;
        case 1:
            sema_down(&OKToLeftForEmergency);
            break;
        case 2:
            sema_down(&OKToRightForVehicle);
            break;
        case 3:
            sema_down(&OKToRightForEmergency);
        default:
            break;
    }
}

/* Print out a debug message upon entrance, sleep the thread for a random amount of time, and print another debug message upon exit.
 */
void CrossBridge(int direc, int prio)
{
    printf("Vehicle enters bridge with direction: %d and priority: %d\n", direc, prio);
    timer_sleep((random_ulong() % 20) + 1);
    printf("Vehicle leaves bridge with direction: %d and priority: %d\n", direc, prio);
}

/* Unblock waithing threads
 */
void ExitBridge(int direc, int prio)
{
    sema_down(&mutex);
    while (num_waiting_emergency_left > 0 
    && num_active_right == 0 && num_active_left < 3)
    {
        sema_up(&OKToLeftForEmergency);
        num_active_left++;
        num_waiting_emergency_left--;
    }
    while (num_waiting_emergency_right > 0 
    && num_active_left == 0 && num_active_right < 3)
    {
        sema_up(&OKToRightForEmergency);
        num_active_right++;
        num_waiting_emergency_right--;
    }
    if (num_waiting_emergency_left + num_waiting_emergency_right == 0)
    {
        while (num_waiting_vehicles_left > 0 
        && num_active_right == 0 && num_active_left < 3)
        {
            sema_up(&OKToLeftForVehicle);
            num_active_left++;
            num_waiting_vehicles_left--;
        }
        while (num_waiting_vehicles_right > 0
        && num_active_left == 0 && num_active_right < 3)
        {
            sema_up(&OKToRightForVehicle);
            num_active_right++;
            num_waiting_vehicles_right--;
        }
    }
    sema_up(&mutex);
}

void OneVehicle(void *arg)
{
    struct car *Car = (struct car*) arg;
    int direc = Car->direc;
    int prio = Car->prio;
    ArriveBridge(direc, prio);
    CrossBridge(direc, prio);
    ExitBridge(direc, prio);
}