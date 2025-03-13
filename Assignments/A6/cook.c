#include "common.h"

// Cleanup IPC resources
void cleanup(int shmid, int semid) {
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}


void cmain(int cook_id, int *shm, int semid) {
    printf("[COOK %c] Started working\n", 'C' + cook_id);
    
    while (1) {
        // Wait for cooking request
        sem_wait(semid, SEM_COOK);
        
        // Check if it's time to leave
        sem_wait(semid, SEM_MUTEX);
        int current_time = shm[TIME];
        int cook_queue_empty = (shm[COOK_QUEUE_FRONT] == shm[COOK_QUEUE_BACK]);
        
        if (current_time >= CLOSING_TIME && cook_queue_empty) {
            // Time to leave, wake up all waiters to terminate
            for (int i = 0; i < NUM_WAITERS; i++) {
                sem_signal(semid, SEM_WAITER_BASE + i);
            }
            sem_signal(semid, SEM_MUTEX);
            break;
        }
        
        // Get cooking request from queue
        int front = shm[COOK_QUEUE_FRONT];
        int waiter_id = shm[COOK_QUEUE_START + 3 * front];
        int customer_id = shm[COOK_QUEUE_START + 3 * front + 1];
        int customer_count = shm[COOK_QUEUE_START + 3 * front + 2];
        
        // Update front of queue
        shm[COOK_QUEUE_FRONT] = (front + 1) % (COOK_QUEUE_SIZE / 3);
        shm[PENDING_ORDER]--;
        
        printf("[COOK %c] Received order for customer %d (party of %d) from waiter %c\n", 
               'C' + cook_id, customer_id, customer_count, 'U' + waiter_id);
        
        sem_signal(semid, SEM_MUTEX);
        
        // Prepare food (simulate cooking time)
        int cooking_time = COOK_TIME_PER_PERSON * customer_count;
        printf("[COOK %c] Cooking for customer %d (will take %d minutes)\n", 
               'C' + cook_id, customer_id, cooking_time);
        
        sem_wait(semid, SEM_MUTEX);
        int cook_start_time = shm[TIME];
        sem_signal(semid, SEM_MUTEX);
        
        simulate_time_passage(shm, cooking_time);
        
        sem_wait(semid, SEM_MUTEX);
        printf("[COOK %c] Finished cooking for customer %d\n", 'C' + cook_id, customer_id);
        
        // Notify waiter
        int waiter_base = WAITER_BASE(waiter_id);
        shm[waiter_base + W_FOOD_READY] = customer_id;
        sem_signal(semid, SEM_MUTEX);
        
        // Signal waiter
        sem_signal(semid, SEM_WAITER_BASE + waiter_id);
    }
    
    printf("[COOK %c] Left for the day\n", 'C' + cook_id);
}



int main() {
    
    key_t key =  ftok(".", 'B');
    
    
    int shmid = shmget(key, SHM_SIZE * sizeof(int), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    

    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (int *)-1) {
        perror("shmat failed");
        exit(1);
    }
    
    // Initialize shared memory
    shm[TIME] = OPENING_TIME;
    shm[EMPTY_TABLES] = MAX_TABLES;
    shm[NEXT_WAITER] = 0;
    shm[PENDING_ORDER] = 0;
    shm[COOK_QUEUE_FRONT] = 0;
    shm[COOK_QUEUE_BACK] = 0;
    
    // Initialize waiter areas
    for (int i = 0; i < NUM_WAITERS; i++) {
        int base = WAITER_BASE(i);
        shm[base + W_FOOD_READY] = 0;
        shm[base + W_PENDING_ORDERS] = 0;
        shm[base + W_QUEUE_FRONT] = 0;
        shm[base + W_QUEUE_BACK] = 0;
    }
    
    // Create semaphores
    int semid = semget(key, SEM_COUNT, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget failed");
        shmdt(shm);
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }
    
    // Initialize semaphores
    union semun arg;
    
    // Initialize mutex semaphore to 1
    arg.val = 1;
    if (semctl(semid, SEM_MUTEX, SETVAL, arg) == -1) {
        perror("semctl failed for mutex");
        cleanup(shmid, semid);
        exit(1);
    }
    
    // Initialize cook semaphore to 0
    arg.val = 0;
    if (semctl(semid, SEM_COOK, SETVAL, arg) == -1) {
        perror("semctl failed for cook");
        cleanup(shmid, semid);
        exit(1);
    }
    
    // Initialize waiter semaphores to 0
    for (int i = 0; i < NUM_WAITERS; i++) {
        arg.val = 0;
        if (semctl(semid, SEM_WAITER_BASE + i, SETVAL, arg) == -1) {
            perror("semctl failed for waiters");
            cleanup(shmid, semid);
            exit(1);
        }
    }
    
    printf("[SYSTEM] Restaurant opens at 11:00am\n");
    
    // Create two cook processes
    pid_t cook_pids[NUM_COOKS];
    
    for (int i = 0; i < 2; i++) {
        cook_pids[i] = fork();
        
        if (cook_pids[i] < 0) {
            perror("fork failed for cook");
            cleanup(shmid, semid);
            exit(1);
        } else if (cook_pids[i] == 0) {
            cmain(i, shm, semid);
            exit(0);
        }
    }
    
    
    for (int i = 0; i < NUM_COOKS; i++) {
        waitpid(cook_pids[i], NULL, 0);
    }
    
    printf("[SYSTEM] All cooks have left\n");
    
    // The customer process is responsible for cleaning up IPC resources
    
    shmdt(shm);
    return 0;
}    

