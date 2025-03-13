#include"common.h"


void cleanup(int shmid, int semid) {
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}


void cmain(int cook_id, int *shm, int semid) {
    printf("[COOK %c] Started working\n", 'C' + cook_id);
    
    while (1) {
        
        sem_wait(semid, 1);
        
        
        sem_wait(semid, 2);
        int current_time = shm[0];
        int cook_queue_empty = (shm[1100] == shm[1101]);
        
        if (current_time >= 240 && cook_queue_empty) {
            
            for (int i = 0; i < 5; i++) {
                sem_signal(semid, 2 + i);
            }
            sem_signal(semid, 2);
            break;
        }
        
        
        int front = shm[1100];
        int waiter_id = shm[1102 + 3 * front];
        int customer_id = shm[1102 + 3 * front + 1];
        int customer_count = shm[1102 + 3 * front + 2];
        
        
        shm[1100] = (front + 1) % (900 / 3);
        shm[3]--;
        
        printf("[COOK %c] Received order for customer %d (party of %d) from waiter %c\n", 
               'C' + cook_id, customer_id, customer_count, 'U' + waiter_id);
        
        sem_signal(semid, 2);
        
        
        int cooking_time = 5 * customer_count;
        printf("[COOK %c] Cooking for customer %d (will take %d minutes)\n", 
               'C' + cook_id, customer_id, cooking_time);
        
        sem_wait(semid, 2);
        int cook_start_time = shm[0];
        sem_signal(semid, 2);
        
        simulate_time_passage(shm, cooking_time);
        
        sem_wait(semid, 2);
        printf("[COOK %c] Finished cooking for customer %d\n", 'C' + cook_id, customer_id);
        
        
        int waiter_base = WAITER_BASE(waiter_id);
        shm[waiter_base + 0] = customer_id;
        sem_signal(semid, 2);
        
        
        sem_signal(semid, 2 + waiter_id);
    }
    
    printf("[COOK %c] Left for the day\n", 'C' + cook_id);
}



int main() {
    
    key_t key =  ftok(".", 'B');
    
    
    int shmid = shmget(key, 2001 * sizeof(int), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    

    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (int *)-1) {
        perror("shmat failed");
        exit(1);
    }
    
    
    shm[0] = 0;
    shm[1] = 10;
    shm[2] = 0;
    shm[3] = 0;
    shm[1100] = 0;
    shm[1101] = 0;
    
    
    for (int i = 0; i < 5; i++) {
        int base = WAITER_BASE(i);
        shm[base + 0] = 0;
        shm[base + 1] = 0;
        shm[base + 2] = 0;
        shm[base + 3] = 0;
    }
    
    
    int semid = semget(key, SEM_COUNT, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget failed");
        shmdt(shm);
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }
    
    
    union semun arg;
    
    
    arg.val = 1;
    if (semctl(semid, 2, SETVAL, arg) == -1) {
        perror("semctl failed for mutex");
        cleanup(shmid, semid);
        exit(1);
    }
    
    
    arg.val = 0;
    if (semctl(semid, 1, SETVAL, arg) == -1) {
        perror("semctl failed for cook");
        cleanup(shmid, semid);
        exit(1);
    }
    
    
    for (int i = 0; i < 5; i++) {
        arg.val = 0;
        if (semctl(semid, 2 + i, SETVAL, arg) == -1) {
            perror("semctl failed for waiters");
            cleanup(shmid, semid);
            exit(1);
        }
    }
    
    printf("[SYSTEM] Restaurant opens at 11:00am\n");
    
    
    pid_t cook_pids[2];
    
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
    
    
    for (int i = 0; i < 2; i++) {
        waitpid(cook_pids[i], NULL, 0);
    }
    
    printf("[SYSTEM] All cooks have left\n");
    
    
    
    shmdt(shm);
    return 0;
}    

