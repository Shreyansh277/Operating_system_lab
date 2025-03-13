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
int *vtime;
int *rtime_array;
int total_visiters;
int done;

int *BA;
int *BC;
int *rtime;

semaphore boat_sem;
semaphore visitor_sem;
pthread_mutex_t the_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t *boat_barriers;
pthread_barrier_t finish_barrier;

void P(semaphore *s) {
    pthread_mutex_lock(&(s->mtx));
    s->value--;
    
    if (s->value < 0) {
        pthread_cond_wait(&(s->cv), &(s->mtx));
    }
    
    pthread_mutex_unlock(&(s->mtx));
}

void V(semaphore *s) {
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
    BA[boat_num] = 1;
    BC[boat_num] = -1;
    pthread_mutex_unlock(&the_mutex);
    
    while (1) {
       
        pthread_mutex_lock(&the_mutex);
        if (done) {
            pthread_mutex_unlock(&the_mutex);
            break;
        }
        pthread_mutex_unlock(&the_mutex);
        
        V(&visitor_sem);
        P(&boat_sem);
        
        pthread_mutex_lock(&the_mutex);
        if (done) {
            pthread_mutex_unlock(&the_mutex);
            break;
        }
        
        
        BA[boat_num] = 1;
        pthread_mutex_unlock(&the_mutex);
        
      
        pthread_barrier_wait(&boat_barriers[boat_num]);
        
        pthread_mutex_lock(&the_mutex);
        
        BA[boat_num] = 0;
        int visitor_num = BC[boat_num];
        int trip_duration = rtime[boat_num];
        
        
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
        total_visiters--;
        int left = total_visiters;
        
        if (left <= 0) {
            done = 1;
            
            
            for (int i = 0; i < m; i++) {
                V(&boat_sem);
            }
            
            for (int j = 0; j < n; j++) {
                V(&visitor_sem);
            }
            
            pthread_mutex_unlock(&the_mutex);
            pthread_barrier_wait(&finish_barrier);
            break;
        }
        pthread_mutex_unlock(&the_mutex);
        
        pthread_mutex_lock(&the_mutex);
        BA[boat_num] = 1;
        BC[boat_num] = -1;
        pthread_mutex_unlock(&the_mutex);
    }
    
    return NULL;
}








void* visitorFunc(void* arg) {
    int visitor_num = *((int *)arg);
    free(arg);
    
    int sight_time = vtime[visitor_num];
    int boat_time = rtime_array[visitor_num];
    
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
    
    V(&boat_sem);
    P(&visitor_sem);
    
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
            if (BA[i] == 1 && BC[i] == -1) {
                BC[i] = visitor_num;
                rtime[i] = boat_time;
                my_boat = i;
                printf("Visitor %d Finds boat %d\n", visitor_num, my_boat);
                fflush(stdout);
                break;
            }
        }
        pthread_mutex_unlock(&the_mutex);
        
        if (my_boat == -1) {
            
            usleep(5000);
            
            pthread_mutex_lock(&the_mutex);
            if (done) {
                pthread_mutex_unlock(&the_mutex);
                return NULL;
            }
            pthread_mutex_unlock(&the_mutex);
        }
    }
    

    pthread_barrier_wait(&boat_barriers[my_boat]);
    
    
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
    
    vtime = (int *)malloc((n + 1) * sizeof(int));
    rtime_array = (int *)malloc((n + 1) * sizeof(int));
    
    for (int i = 1; i <= n; i++) {
        vtime[i] = 30 + rand() % 91;
        rtime_array[i] = 15 + rand() % 46;
    }
    
    BA = (int *)calloc(m + 1, sizeof(int));
    BC = (int *)malloc((m + 1) * sizeof(int));
    rtime = (int *)malloc((m + 1) * sizeof(int));
    boat_barriers = (pthread_barrier_t *)malloc((m + 1) * sizeof(pthread_barrier_t));
    
    for (int i = 1; i <= m; i++) {
        BC[i] = -1;
    }
    
    done = 0;
    total_visiters = n;
    
    
    pthread_mutex_init(&boat_sem.mtx, NULL);
    pthread_cond_init(&boat_sem.cv, NULL);
    boat_sem.value = 0;
    
    pthread_mutex_init(&visitor_sem.mtx, NULL);
    pthread_cond_init(&visitor_sem.cv, NULL);
    visitor_sem.value = 0;
    
    
    pthread_barrier_init(&finish_barrier, NULL, 2);
    
    
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
    
    
    pthread_barrier_wait(&finish_barrier);
    printf("All visitors have completed their rides. Shutting down...\n");
    fflush(stdout);
    
    
    for (int i = 1; i <= m; i++) {
        pthread_join(boat_threads[i], NULL);
    }
    
    for (int i = 1; i <= n; i++) {
        pthread_join(visitor_threads[i], NULL);
    }
    
    
    free(boat_threads);
    free(visitor_threads);
    free(vtime);
    free(rtime_array);
    free(BA);
    free(BC);
    free(rtime);
    
    for (int i = 1; i <= m; i++) {
        pthread_barrier_destroy(&boat_barriers[i]);
    }
    free(boat_barriers);
    pthread_barrier_destroy(&finish_barrier);
    
    
    pthread_mutex_destroy(&boat_sem.mtx);
    pthread_cond_destroy(&boat_sem.cv);
    pthread_mutex_destroy(&visitor_sem.mtx);
    pthread_cond_destroy(&visitor_sem.cv);
    
    fflush(stdout);
    return 0;
}