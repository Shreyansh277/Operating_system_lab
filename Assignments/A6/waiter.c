#include "common.h"


// Waiter process function
void wmain(int waiter_id, int *shm, int semid) {
    printf("[WAITER %c] Started working\n", 'U' + waiter_id);
    
    int waiter_base = WAITER_BASE(waiter_id);
    
    while (1) {
        // Wait for signal from cook or customer
        sem_wait(semid, SEM_WAITER_BASE + waiter_id);
        
        // Check if it's time to leave
        sem_wait(semid, SEM_MUTEX);
        int current_time = shm[TIME];
        int pending_orders = shm[waiter_base + W_PENDING_ORDERS];
        int food_ready = shm[waiter_base + W_FOOD_READY];
        
        if (current_time >= CLOSING_TIME && pending_orders == 0 && food_ready == 0) {
            sem_signal(semid, SEM_MUTEX);
            break;
        }
        
        // Check if food is ready for a customer
        if (food_ready != 0) {
            int customer_id = food_ready;
            
            // Reset food ready flag
            shm[waiter_base + W_FOOD_READY] = 0;
            
            printf("[WAITER %c] Serving food to customer %d\n", 'U' + waiter_id, customer_id);
            
            // Release mutex before signaling customer
            sem_signal(semid, SEM_MUTEX);
            
            // Signal customer that food is ready
            sem_signal(semid, SEM_CUSTOMER_BASE + customer_id);
        }
        // Check if there are pending customer orders
        else if (pending_orders > 0) {
            // Get customer from queue
            int front = shm[waiter_base + W_QUEUE_FRONT];
            int customer_id = shm[waiter_base + W_QUEUE_START + 2 * front];
            int customer_count = shm[waiter_base + W_QUEUE_START + 2 * front + 1];
            
            // Update queue
            shm[waiter_base + W_QUEUE_FRONT] = (front + 1) % (WAITER_SIZE / 2);
            shm[waiter_base + W_PENDING_ORDERS]--;
            
            printf("[WAITER %c] Taking order from customer %d (party of %d)\n", 
                   'U' + waiter_id, customer_id, customer_count);
            
            // Release mutex before time-consuming operation
            sem_signal(semid, SEM_MUTEX);
            
            // Take order (1 minute)
            simulate_time_passage(shm, ORDER_TIME);
            
            // Queue cooking request
            sem_wait(semid, SEM_MUTEX);
            
            // Add to cook queue
            int back = shm[COOK_QUEUE_BACK];
            shm[COOK_QUEUE_START + 3 * back] = waiter_id;
            shm[COOK_QUEUE_START + 3 * back + 1] = customer_id;
            shm[COOK_QUEUE_START + 3 * back + 2] = customer_count;
            
            // Update back of queue
            shm[COOK_QUEUE_BACK] = (back + 1) % (COOK_QUEUE_SIZE / 3);
            shm[PENDING_ORDER]++;
            
            printf("[WAITER %c] Placed order for customer %d to cook queue\n", 
                   'U' + waiter_id, customer_id);
            
            // Signal cook
            sem_signal(semid, SEM_MUTEX);
            sem_signal(semid, SEM_COOK);
        } else {
            // No work to do, release mutex
            sem_signal(semid, SEM_MUTEX);
        }
    }
    
    printf("[WAITER %c] Left for the day\n", 'U' + waiter_id);
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
    
    printf("[SYSTEM] Waiters are ready to serve\n");
    
    // Create waiter processes
    pid_t waiter_pids[NUM_WAITERS];
    char waiter_names[NUM_WAITERS] = {'U', 'V', 'W', 'X', 'Y'};
    
    for (int i = 0; i < NUM_WAITERS; i++) {
        waiter_pids[i] = fork();
        
        if (waiter_pids[i] < 0) {
            perror("fork failed for waiter");
            shmdt(shm);
            exit(1);
        } else if (waiter_pids[i] == 0) {
            // Child process (waiter)
            wmain(i, shm, semid);
            exit(0);  // Child should not return to main
        }
    }
    
    // Parent waits for all waiters to finish
    for (int i = 0; i < NUM_WAITERS; i++) {
        waitpid(waiter_pids[i], NULL, 0);
    }
    
    printf("[SYSTEM] All waiters have left\n");
    
    shmdt(shm);
    return 0;
}

