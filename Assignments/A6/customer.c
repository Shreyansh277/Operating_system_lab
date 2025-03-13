#include "common.h"

// Customer process function
void cmain(int customer_id, int arrival_time, int customer_count, int *shm, int semid) {
    printf("[CUSTOMER %d] Arrived at time %d (party of %d)\n", 
           customer_id, arrival_time, customer_count);
    
    // Check if the restaurant is closed
    sem_wait(semid, SEM_MUTEX);
    if (shm[TIME] >= CLOSING_TIME) {
        printf("[CUSTOMER %d] Restaurant is closed. Leaving.\n", customer_id);
        sem_signal(semid, SEM_MUTEX);
        return;
    }
    
    // Check if there are any empty tables
    if (shm[EMPTY_TABLES] <= 0) {
        printf("[CUSTOMER %d] No empty tables. Leaving.\n", customer_id);
        sem_signal(semid, SEM_MUTEX);
        return;
    }
    
    // Occupy a table
    shm[EMPTY_TABLES]--;
    
    // Find the next waiter to serve
    int waiter_id = shm[NEXT_WAITER];
    shm[NEXT_WAITER] = (waiter_id + 1) % NUM_WAITERS;
    
    int waiter_base = WAITER_BASE(waiter_id);
    
    printf("[CUSTOMER %d] Occupied a table. Served by waiter %c\n", 
           customer_id, 'U' + waiter_id);
    
    // Add to waiter's queue
    int back = shm[waiter_base + W_QUEUE_BACK];
    shm[waiter_base + W_QUEUE_START + 2 * back] = customer_id;
    shm[waiter_base + W_QUEUE_START + 2 * back + 1] = customer_count;
    
    // Update queue
    shm[waiter_base + W_QUEUE_BACK] = (back + 1) % (WAITER_SIZE / 2);
    shm[waiter_base + W_PENDING_ORDERS]++;
    
    printf("[CUSTOMER %d] Placed in waiter %c's queue\n", customer_id, 'U' + waiter_id);
    
    // Signal waiter
    sem_signal(semid, SEM_MUTEX);
    sem_signal(semid, SEM_WAITER_BASE + waiter_id);
    
    // Wait for food to be served
    printf("[CUSTOMER %d] Waiting for food\n", customer_id);
    sem_wait(semid, SEM_CUSTOMER_BASE + customer_id);
    
    // Food is served, start eating
    sem_wait(semid, SEM_MUTEX);
    printf("[CUSTOMER %d] Received food. Starting to eat.\n", customer_id);
    sem_signal(semid, SEM_MUTEX);
    
    // Eat food (30 minutes)
    simulate_time_passage(shm, EATING_TIME);
    
    // Finish eating and leave
    sem_wait(semid, SEM_MUTEX);
    
    // Free the table
    shm[EMPTY_TABLES]++;
    
    printf("[CUSTOMER %d] Finished eating. Leaving the restaurant.\n", customer_id);
    sem_signal(semid, SEM_MUTEX);
}


int main() {
    // Generate key for IPC resources
    key_t key =  ftok(".", 'B');
    
    // Attach to shared memory
    int shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    
    // Attach to shared memory
    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (int *)-1) {
        perror("shmat failed");
        exit(1);
    }
    
    // Connect to semaphores
    int semid = semget(key, SEM_COUNT, 0666);
    if (semid == -1) {
        perror("semget failed");
        shmdt(shm);
        exit(1);
    }
    
    // Open customer file
    FILE *fp = fopen("customers.txt", "r");
    if (fp == NULL) {
        perror("Failed to open customers.txt");
        shmdt(shm);
        exit(1);
    }
    
    printf("[SYSTEM] Starting customer simulation\n");
    
    // Read customer data
    int customer_id, arrival_time, customer_count;
    int prev_arrival_time = 0;
    pid_t *customer_pids = NULL;
    int num_customers = 0;
    
    while (fscanf(fp, "%d %d %d", &customer_id, &arrival_time, &customer_count) == 3) {
        if (customer_id < 0) {
            break;  // End of file marker
        }
        
        // Wait for customer's arrival time
        if (arrival_time > prev_arrival_time) {
            int time_diff = arrival_time - prev_arrival_time;
            
            sem_wait(semid, SEM_MUTEX);
            int current_time = shm[TIME];
            
            // If current time is ahead of arrival time, don't wait
            if (current_time < arrival_time) {
                sem_signal(semid, SEM_MUTEX);
                simulate_time_passage(shm, time_diff);
            } else {
                sem_signal(semid, SEM_MUTEX);
            }
        }
        
        prev_arrival_time = arrival_time;
        
        // Check if the restaurant is closed
        sem_wait(semid, SEM_MUTEX);
        if (shm[TIME] >= CLOSING_TIME) {
            printf("[SYSTEM] Restaurant is closed. Customer %d cannot enter.\n", customer_id);
            sem_signal(semid, SEM_MUTEX);
            continue;
        }
        sem_signal(semid, SEM_MUTEX);
        
        // Fork a new customer process
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork failed for customer");
            continue;
        } else if (pid == 0) {
            // Child process (customer)
            cmain(customer_id, arrival_time, customer_count, shm, semid);
            fclose(fp);
            exit(0);  // Child should not return to main
        }
        
        // Parent process: keep track of customer pids
        num_customers++;
        customer_pids = (pid_t *)realloc(customer_pids, num_customers * sizeof(pid_t));
        customer_pids[num_customers - 1] = pid;
    }
    
    fclose(fp);
    
    // Wait for all customers to finish
    for (int i = 0; i < num_customers; i++) {
        waitpid(customer_pids[i], NULL, 0);
    }
    
    free(customer_pids);
    
    printf("[SYSTEM] All customers have left\n");
    
    // Clean up IPC resources
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    
    return 0;
}

