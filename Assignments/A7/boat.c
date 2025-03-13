#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;

int m;
int n;
int *visitTimes;
int *rideTimesArray;
int remaining_visitors;
int done;

int *boatAvailable;
int *boatCustomer;
int *rideTime;

semaphore boat_sem;
semaphore visitor_sem;
pthread_mutex_t the_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t *boat_barriers;
pthread_barrier_t finish_barrier;

void waitSemaphore(semaphore *s) {
    pthread_mutex_lock(&(s->mtx));
    s->value--;
    
    if (s->value < 0) {
        pthread_cond_wait(&(s->cv), &(s->mtx));
    }
    
    pthread_mutex_unlock(&(s->mtx));
}

void signalSemaphore(semaphore *s) {
    pthread_mutex_lock(&(s->mtx));
    s->value++;
    
    if (s->value <= 0) {
        pthread_cond_signal(&(s->cv));
    }
    
    pthread_mutex_unlock(&(s->mtx));
}

void* boatFunc(void* arg) {
    int boat_num = *((int *)arg);
    free(arg);
    
    printf("Boat %d Ready\n", boat_num);
    fflush(stdout);
    
    pthread_barrier_init(&boat_barriers[boat_num], NULL, 2);
    
    pthread_mutex_lock(&the_mutex);
    boatAvailable[boat_num] = 1;
    boatCustomer[boat_num] = -1;
    pthread_mutex_unlock(&the_mutex);
    
    while (1) {
       
        pthread_mutex_lock(&the_mutex);
        if (done) {
            pthread_mutex_unlock(&the_mutex);
            break;
        }
        pthread_mutex_unlock(&the_mutex);
        
        signalSemaphore(&visitor_sem);
        waitSemaphore(&boat_sem);
        
        pthread_mutex_lock(&the_mutex);
        if (done) {
            pthread_mutex_unlock(&the_mutex);
            break;
        }
        
        
        boatAvailable[boat_num] = 1;
        pthread_mutex_unlock(&the_mutex);
        
      
        pthread_barrier_wait(&boat_barriers[boat_num]);
        
        pthread_mutex_lock(&the_mutex);
        
        boatAvailable[boat_num] = 0;
        int visitor_num = boatCustomer[boat_num];
        int trip_duration = rideTime[boat_num];
        
        
        if (visitor_num <= 0 || visitor_num > n) {
            printf("ERROR: Invalid visitor number %d for boat %d\n", visitor_num, boat_num);
            fflush(stdout);
            pthread_mutex_unlock(&the_mutex);
            continue;
        }
        pthread_mutex_unlock(&the_mutex);
        
        printf("Boat %d Start of ride for visitor %d\n", boat_num, visitor_num);
        fflush(stdout);
        
        usleep(trip_duration * 100000);
        
        printf("Boat %d End of ride for visitor %d (ride time = %d)\n", 
               boat_num, visitor_num, trip_duration);
               fflush(stdout);

               printf("Visitor %d Leaving\n", visitor_num);
               fflush(stdout);
        
        pthread_mutex_lock(&the_mutex);
        remaining_visitors--;
        int left = remaining_visitors;
        
        if (left <= 0) {
            done = 1;
            
            
            for (int i = 0; i < m; i++) {
                signalSemaphore(&boat_sem);
            }
            
            for (int j = 0; j < n; j++) {
                signalSemaphore(&visitor_sem);
            }
            
            pthread_mutex_unlock(&the_mutex);
            pthread_barrier_wait(&finish_barrier);
            break;
        }
        pthread_mutex_unlock(&the_mutex);
        
        pthread_mutex_lock(&the_mutex);
        boatAvailable[boat_num] = 1;
        boatCustomer[boat_num] = -1;
        pthread_mutex_unlock(&the_mutex);
    }
    
    return NULL;
}








void* visitorFunc(void* arg) {
    int visitor_num = *((int *)arg);
    free(arg);
    
    int sight_time = visitTimes[visitor_num];
    int boat_time = rideTimesArray[visitor_num];
    
    printf("Visitor %d Starts sightseeing for %d minutes\n", visitor_num, sight_time);
    fflush(stdout);
    
    usleep(sight_time * 100000);
    
    pthread_mutex_lock(&the_mutex);
    if (done) {
        pthread_mutex_unlock(&the_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&the_mutex);
    
    printf("Visitor %d Ready to ride a boat (ride time = %d)\n", visitor_num, boat_time);
    fflush(stdout);
    
    signalSemaphore(&boat_sem);
    waitSemaphore(&visitor_sem);
    
    pthread_mutex_lock(&the_mutex);
    if (done) {
        pthread_mutex_unlock(&the_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&the_mutex);
    
    int my_boat = -1;
    
    while (my_boat == -1) {
        pthread_mutex_lock(&the_mutex);
        if (done) {
            pthread_mutex_unlock(&the_mutex);
            return NULL;
        }
        
        for (int i = 1; i <= m; i++) {
            if (boatAvailable[i] == 1 && boatCustomer[i] == -1) {
                boatCustomer[i] = visitor_num;
                rideTime[i] = boat_time;
                my_boat = i;
                printf("Visitor %d Finds boat %d\n", visitor_num, my_boat);
                fflush(stdout);
                break;
            }
        }
        pthread_mutex_unlock(&the_mutex);
        
        if (my_boat == -1) {
            // If no boat found, sleep a short time to avoid busy waiting
            usleep(5000);
            
            pthread_mutex_lock(&the_mutex);
            if (done) {
                pthread_mutex_unlock(&the_mutex);
                return NULL;
            }
            pthread_mutex_unlock(&the_mutex);
        }
    }
    
    // Wait at the barrier to synchronize with the boat
    pthread_barrier_wait(&boat_barriers[my_boat]);
    
    // No need to wait here as the boat is already handling the ride
    // usleep(rideTimesArray[visitor_num] * 100000);
    
    // printf("Visitor %d Leaving\n", visitor_num);
    // fflush(stdout);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <boats> <visitors>\n", argv[0]);
        return 1;
    }
    
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    
    if (m < 5 || m > 10) {
        printf("Boats must be between 5 and 10\n");
        return 1;
    }
    if (n < 20 || n > 100) {
        printf("Visitors must be between 20 and 100\n");
        return 1;
    }
    fflush(stdout);
    srand(time(NULL));
    
    visitTimes = (int *)malloc((n + 1) * sizeof(int));
    rideTimesArray = (int *)malloc((n + 1) * sizeof(int));
    
    for (int i = 1; i <= n; i++) {
        visitTimes[i] = 30 + rand() % 91;
        rideTimesArray[i] = 15 + rand() % 46;
    }
    
    boatAvailable = (int *)calloc(m + 1, sizeof(int));
    boatCustomer = (int *)malloc((m + 1) * sizeof(int));
    rideTime = (int *)malloc((m + 1) * sizeof(int));
    boat_barriers = (pthread_barrier_t *)malloc((m + 1) * sizeof(pthread_barrier_t));
    
    for (int i = 1; i <= m; i++) {
        boatCustomer[i] = -1;
    }
    
    done = 0;
    remaining_visitors = n;
    
    // Initialize semaphores properly
    pthread_mutex_init(&boat_sem.mtx, NULL);
    pthread_cond_init(&boat_sem.cv, NULL);
    boat_sem.value = 0;
    
    pthread_mutex_init(&visitor_sem.mtx, NULL);
    pthread_cond_init(&visitor_sem.cv, NULL);
    visitor_sem.value = 0;
    
    // Initialize the finish barrier with the main thread + 1 (for the last boat)
    pthread_barrier_init(&finish_barrier, NULL, 2);
    
    // Allocate thread arrays with proper indexing
    pthread_t *boat_threads = (pthread_t *)malloc((m+1) * sizeof(pthread_t));
    for (int i = 1; i <= m; i++) {
        int *id = (int *)malloc(sizeof(int));
        *id = i;
        pthread_create(&boat_threads[i], NULL, boatFunc, (void *)id);
    }
    
    pthread_t *visitor_threads = (pthread_t *)malloc((n+1) * sizeof(pthread_t));
    for (int i = 1; i <= n; i++) {
        int *id = (int *)malloc(sizeof(int));
        *id = i;
        pthread_create(&visitor_threads[i], NULL, visitorFunc, (void *)id);
    }
    
    // Main thread waits for completion
    pthread_barrier_wait(&finish_barrier);
    printf("All visitors have completed their rides. Shutting down...\n");
    fflush(stdout);
    
    // Join all threads properly - match our indexing (starting at 1)
    for (int i = 1; i <= m; i++) {
        pthread_join(boat_threads[i], NULL);
    }
    
    for (int i = 1; i <= n; i++) {
        pthread_join(visitor_threads[i], NULL);
    }
    
    // Clean up resources
    free(boat_threads);
    free(visitor_threads);
    free(visitTimes);
    free(rideTimesArray);
    free(boatAvailable);
    free(boatCustomer);
    free(rideTime);
    
    for (int i = 1; i <= m; i++) {
        pthread_barrier_destroy(&boat_barriers[i]);
    }
    free(boat_barriers);
    pthread_barrier_destroy(&finish_barrier);
    
    // Properly destroy semaphores
    pthread_mutex_destroy(&boat_sem.mtx);
    pthread_cond_destroy(&boat_sem.cv);
    pthread_mutex_destroy(&visitor_sem.mtx);
    pthread_cond_destroy(&visitor_sem.cv);
    
    printf("Program completed successfully\n");
    fflush(stdout);
    return 0;
}