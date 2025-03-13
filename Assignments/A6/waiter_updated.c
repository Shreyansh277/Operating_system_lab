#include"common.h"



void wmain(int waiter_id, int *shm, int semid) {
    printf("[WAITER %c] Started working\n", 'U' + waiter_id);
    
    int waiter_base = WAITER_BASE(waiter_id);
    
    while (1) {
        
        sem_wait(semid, 2 + waiter_id);
        
        
        sem_wait(semid, 0);
        int current_time = shm[0];
        int pending_orders = shm[waiter_base + 1];
        int food_ready = shm[waiter_base + 0];
        
        if (current_time >= 240 && pending_orders == 0 && food_ready == 0) {
            sem_signal(semid, 0);
            break;
        }
        
        
        if (food_ready != 0) {
            int customer_id = food_ready;
            
            
            shm[waiter_base + 0] = 0;
            
            printf("[WAITER %c] Serving food to customer %d\n", 'U' + waiter_id, customer_id);
            
            
            sem_signal(semid, 0);
            
            
            sem_signal(semid, 7 + customer_id);
        }
        
        else if (pending_orders > 0) {
            
            int front = shm[waiter_base + 2];
            int customer_id = shm[waiter_base + 4 + 2 * front];
            int customer_count = shm[waiter_base + 4 + 2 * front + 1];
            
            
            shm[waiter_base + 2] = (front + 1) % (200 / 2);
            shm[waiter_base + 1]--;
            
            printf("[WAITER %c] Taking order from customer %d (party of %d)\n", 
                   'U' + waiter_id, customer_id, customer_count);
            
            
            sem_signal(semid, 0);
            
            
            simulate_time_passage(shm, 1);
            
            
            sem_wait(semid, 0);
            
            
            int back = shm[1101];
            shm[1102 + 3 * back] = waiter_id;
            shm[1102 + 3 * back + 1] = customer_id;
            shm[1102 + 3 * back + 2] = customer_count;
            
            
            shm[1101] = (back + 1) % (900 / 3);
            shm[3]++;
            
            printf("[WAITER %c] Placed order for customer %d to cook queue\n", 
                   'U' + waiter_id, customer_id);
            
            
            sem_signal(semid, 0);
            sem_signal(semid, 1);
        } else {
            
            sem_signal(semid, 0);
        }
    }
    
    printf("[WAITER %c] Left for the day\n", 'U' + waiter_id);
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
    
    printf("[SYSTEM] Waiters are ready to serve\n");
    
    
    pid_t waiter_pids[5];
    char waiter_names[5] = {'U', 'V', 'W', 'X', 'Y'};
    
    for (int i = 0; i < 5; i++) {
        waiter_pids[i] = fork();
        
        if (waiter_pids[i] < 0) {
            perror("fork failed for waiter");
            shmdt(shm);
            exit(1);
        } else if (waiter_pids[i] == 0) {
            
            wmain(i, shm, semid);
            exit(0);  
        }
    }
    
    
    for (int i = 0; i < 5; i++) {
        waitpid(waiter_pids[i], NULL, 0);
    }
    
    printf("[SYSTEM] All waiters have left\n");
    
    shmdt(shm);
    return 0;
}

