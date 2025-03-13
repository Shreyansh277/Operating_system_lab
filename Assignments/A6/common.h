#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

// Shared memory layout
#define SHM_SIZE 2001      
#define GLOBAL_VARS 100    
#define WAITER_SIZE 200    
#define COOK_QUEUE_SIZE 900

// Global variable indices in shared memory
#define TIME 0         
#define EMPTY_TABLES 1 
#define NEXT_WAITER 2  
#define PENDING_ORDER 3 // Number of pending cooking orders

// Maximum values
#define MAX_CUSTOMERS 200  // Maximum number of customers per day
#define MAX_TABLES 10      // Maximum number of tables in the restaurant

// Waiter data structure indices (relative to waiter's base address)
#define W_FOOD_READY 0     // Customer ID for which food is ready
#define W_PENDING_ORDERS 1 // Number of customers waiting to place order
#define W_QUEUE_FRONT 2    // Front index of waiter's queue
#define W_QUEUE_BACK 3     // Back index of waiter's queue
#define W_QUEUE_START 4    // Start of waiter's customer queue

// Waiter base address computation
#define WAITER_BASE(id) (100 + (id) * 200)

// Cook queue indices
#define COOK_QUEUE_FRONT 1100  // Front index of cook queue
#define COOK_QUEUE_BACK 1101   // Back index of cook queue
#define COOK_QUEUE_START 1102  // Start of cook queue data

// Time constants (in minutes)
#define OPENING_TIME 0      // 11:00am
#define CLOSING_TIME 240    // 3:00pm (240 minutes after 11:00am)
#define ORDER_TIME 1        // Time to take an order
#define COOK_TIME_PER_PERSON 5 // Cooking time per person
#define EATING_TIME 30      // Time to eat

// Time scaling
#define MINUTE_SCALE 100000 // Scale each minute to 100ms (in microseconds)

// Semaphore indices
#define SEM_MUTEX 0         // Protect shared memory access
#define SEM_COOK 1          // For cooks
#define SEM_WAITER_BASE 2   // Base index for waiter semaphores
#define SEM_CUSTOMER_BASE 7 // Base index for customer semaphores

// Total number of semaphores
#define SEM_COUNT 207

// Number of waiters and cooks
#define NUM_WAITERS 5
#define NUM_COOKS 2

// Waiter IDs
#define WAITER_U 0
#define WAITER_V 1
#define WAITER_W 2
#define WAITER_X 3
#define WAITER_Y 4

// Semaphore operations
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Helper functions for semaphores
void sem_wait(int semid, int sem_num) {
    struct sembuf sb = {sem_num, -1, 0};
    if (semop(semid, &sb, 1) == -1) {
        exit(1);
    }
}

void sem_signal(int semid, int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};
    if (semop(semid, &sb, 1) == -1) {
        exit(1);
    }
}

// Set simulated time
void set_time(int *shm, int new_time) {
    if (new_time < shm[0]) {
        fprintf(stderr, "Warning: Attempt to set time backwards from %d to %d. Ignored.\n", 
                shm[0], new_time);
        return;
    }
    shm[0] = new_time;
}

// Simulate passage of time
void simulate_time_passage(int *shm, int minutes) {
    int curr_time = shm[0];
    usleep(minutes * 100000); // Scale each minute to 100ms
    set_time(shm, curr_time + minutes);
}

#endif /* COMMON_H */