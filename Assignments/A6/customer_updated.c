#include"common.h"


void cmain(int customer_id, int arrival_time, int customer_count, int *shm, int semid) {
    printf("[CUSTOMER %d] Arrived at time %d (party of %d)\n", 
           customer_id, arrival_time, customer_count);
    
    
    sem_wait(semid, 0);
    if (shm[0] >= 240) {
        printf("[CUSTOMER %d] Restaurant is closed. Leaving.\n", customer_id);
        sem_signal(semid, 0);
        return;
    }
    
    
    if (shm[1] <= 0) {
        printf("[CUSTOMER %d] No empty tables. Leaving.\n", customer_id);
        sem_signal(semid, 0);
        return;
    }
    
    
    shm[1]--;
    
    
    int waiter_id = shm[2];
    shm[2] = (waiter_id + 1) % 5;
    
    int waiter_base = WAITER_BASE(waiter_id);
    
    printf("[CUSTOMER %d] Occupied a table. Served by waiter %c\n", 
           customer_id, 'U' + waiter_id);
    
    
    int back = shm[waiter_base + 3];
    shm[waiter_base + 4 + 2 * back] = customer_id;
    shm[waiter_base + 4 + 2 * back + 1] = customer_count;
    
    
    shm[waiter_base + 3] = (back + 1) % (200 / 2);
    shm[waiter_base + 1]++;
    
    printf("[CUSTOMER %d] Placed in waiter %c's queue\n", customer_id, 'U' + waiter_id);
    
    
    sem_signal(semid, 0);
    sem_signal(semid, 2 + waiter_id);
    
    
    printf("[CUSTOMER %d] Waiting for food\n", customer_id);
    sem_wait(semid, 7 + customer_id);
    
    
    sem_wait(semid, 0);
    printf("[CUSTOMER %d] Received food. Starting to eat.\n", customer_id);
    sem_signal(semid, 0);
    
    
    simulate_time_passage(shm, 30);
    
    
    sem_wait(semid, 0);
    
    
    shm[1]++;
    
    printf("[CUSTOMER %d] Finished eating. Leaving the restaurant.\n", customer_id);
    sem_signal(semid, 0);
}


int main() {
    
    key_t key =  ftok(".", 'B');
    
    
    int shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    
    
    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (int *)-1) {
        perror("shmat failed");
        exit(1);
    }
    
    
    int semid = semget(key, SEM_COUNT, 0666);
    if (semid == -1) {
        perror("semget failed");
        shmdt(shm);
        exit(1);
    }
    
    
    FILE *fp = fopen("customers.txt", "r");
    if (fp == NULL) {
        perror("Failed to open customers.txt");
        shmdt(shm);
        exit(1);
    }
    
    printf("[SYSTEM] Starting customer simulation\n");
    
    
    int customer_id, arrival_time, customer_count;
    int prev_arrival_time = 0;
    pid_t *customer_pids = NULL;
    int num_customers = 0;
    
    while (fscanf(fp, "%d %d %d", &customer_id, &arrival_time, &customer_count) == 3) {
        if (customer_id < 0) {
            break;  
        }
        
        
        if (arrival_time > prev_arrival_time) {
            int time_diff = arrival_time - prev_arrival_time;
            
            sem_wait(semid, 0);
            int current_time = shm[0];
            
            
            if (current_time < arrival_time) {
                sem_signal(semid, 0);
                simulate_time_passage(shm, time_diff);
            } else {
                sem_signal(semid, 0);
            }
        }
        
        prev_arrival_time = arrival_time;
        
        
        sem_wait(semid, 0);
        if (shm[0] >= 240) {
            printf("[SYSTEM] Restaurant is closed. Customer %d cannot enter.\n", customer_id);
            sem_signal(semid, 0);
            continue;
        }
        sem_signal(semid, 0);
        
        
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork failed for customer");
            continue;
        } else if (pid == 0) {
            
            cmain(customer_id, arrival_time, customer_count, shm, semid);
            fclose(fp);
            exit(0);  
        }
        
        
        num_customers++;
        customer_pids = (pid_t *)realloc(customer_pids, num_customers * sizeof(pid_t));
        customer_pids[num_customers - 1] = pid;
    }
    
    fclose(fp);
    
    
    for (int i = 0; i < num_customers; i++) {
        waitpid(customer_pids[i], NULL, 0);
    }
    
    free(customer_pids);
    
    printf("[SYSTEM] All customers have left\n");
    
    
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    
    return 0;
}

