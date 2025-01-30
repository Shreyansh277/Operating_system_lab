#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX_PROCESSES 1000
#define MAX_BURSTS 100

// Process states
typedef enum {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} ProcessState;

// Event types
typedef enum {
    PROCESS_ARRIVAL,
    CPU_BURST_COMPLETION,
    IO_BURST_COMPLETION,
    CPU_QUANTUM_TIMEOUT
} EventType;

// Process Control Block
typedef struct {
    int id;
    int arrival_time;
    int *cpu_bursts;
    int *io_bursts;
    int num_bursts;
    int current_burst;
    ProcessState state;
    int remaining_time;    // Remaining time in current burst
    int wait_time;
    int turnaround_time;
    int total_execution_time;
    int last_ready_time;   // Time when process last entered ready queue
} PCB;

// Event structure
typedef struct {
    int time;
    int process_id;
    EventType type;
} Event;

// Global variables
PCB processes[MAX_PROCESSES];
int num_processes = 0;
Event event_queue[MAX_PROCESSES * MAX_BURSTS];
int event_queue_size = 0;
int ready_queue[MAX_PROCESSES];
int ready_queue_front = 0, ready_queue_rear = 0;
int current_time = 0;
int current_process = -1;
int cpu_idle_time = 0;

// Function declarations
void read_processes(const char *filename);
void init_scheduler(int quantum);
void run_scheduler(int quantum);
void handle_event();
void add_event(int time, int process_id, EventType type);
void add_to_ready_queue(int process_id);
int remove_from_ready_queue();
void add_to_event_queue(Event event);
Event remove_from_event_queue();
void print_stats();

// Min-heap operations for event queue
void swap_events(Event *a, Event *b) {
    Event temp = *a;
    *a = *b;
    *b = temp;
}

void heapify_up(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (event_queue[parent].time > event_queue[index].time ||
            (event_queue[parent].time == event_queue[index].time && 
             event_queue[parent].process_id > event_queue[index].process_id)) {
            swap_events(&event_queue[parent], &event_queue[index]);
            index = parent;
        } else {
            break;
        }
    }
}

void heapify_down(int index) {
    while (1) {
        int smallest = index;
        int left = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < event_queue_size && 
            (event_queue[left].time < event_queue[smallest].time ||
             (event_queue[left].time == event_queue[smallest].time && 
              event_queue[left].process_id < event_queue[smallest].process_id))) {
            smallest = left;
        }

        if (right < event_queue_size && 
            (event_queue[right].time < event_queue[smallest].time ||
             (event_queue[right].time == event_queue[smallest].time && 
              event_queue[right].process_id < event_queue[smallest].process_id))) {
            smallest = right;
        }

        if (smallest != index) {
            swap_events(&event_queue[index], &event_queue[smallest]);
            index = smallest;
        } else {
            break;
        }
    }
}

void add_to_event_queue(Event event) {
    event_queue[event_queue_size] = event;
    heapify_up(event_queue_size);
    event_queue_size++;
}

Event remove_from_event_queue() {
    Event min = event_queue[0];
    event_queue[0] = event_queue[--event_queue_size];
    if (event_queue_size > 0) {
        heapify_down(0);
    }
    return min;
}

// Ready queue operations
void add_to_ready_queue(int process_id) {
    PCB *p = &processes[process_id];
    p->last_ready_time = current_time;  // Record when process enters ready queue
    ready_queue[ready_queue_rear++] = process_id;
}

int remove_from_ready_queue() {
    if (ready_queue_front == ready_queue_rear) return -1;
    int process_id = ready_queue[ready_queue_front++];
    PCB *p = &processes[process_id];
    // Add waiting time when process leaves ready queue
    p->wait_time += current_time - p->last_ready_time;
    return process_id;
}

// File reading
void read_processes(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error opening file: %s\n", filename);
        exit(1);
    }

    fscanf(fp, "%d", &num_processes);
    
    for (int i = 0; i < num_processes; i++) {
        PCB *p = &processes[i];
        p->cpu_bursts = malloc(MAX_BURSTS * sizeof(int));
        p->io_bursts = malloc(MAX_BURSTS * sizeof(int));
        
        fscanf(fp, "%d %d", &p->id, &p->arrival_time);
        
        int j = 0;
        while (1) {
            fscanf(fp, "%d", &p->cpu_bursts[j]);
            fscanf(fp, "%d", &p->io_bursts[j]);
            if (p->io_bursts[j] == -1) break;
            j++;
        }
        
        p->num_bursts = j + 1;
        p->current_burst = 0;
        p->state = NEW;
        p->wait_time = 0;
        p->turnaround_time = 0;
        p->total_execution_time = 0;
        p->last_ready_time = 0;
        p->remaining_time = 0;
        
        // Calculate total execution time
        for (int k = 0; k < p->num_bursts; k++) {
            p->total_execution_time += p->cpu_bursts[k];
            if (k < p->num_bursts - 1) {
                p->total_execution_time += p->io_bursts[k];
            }
        }
        
        // Add initial arrival event
        Event e = {p->arrival_time, i, PROCESS_ARRIVAL};
        add_to_event_queue(e);
    }
    
    fclose(fp);
}

void init_scheduler(int quantum) {
    current_time = 0;
    current_process = -1;
    cpu_idle_time = 0;
    event_queue_size = 0;
    ready_queue_front = ready_queue_rear = 0;
    
    for (int i = 0; i < num_processes; i++) {
        processes[i].current_burst = 0;
        processes[i].state = NEW;
        processes[i].wait_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].last_ready_time = 0;
        processes[i].remaining_time = 0;
        
        Event e = {processes[i].arrival_time, i, PROCESS_ARRIVAL};
        add_to_event_queue(e);
    }
    return;
}

void schedule_process(int quantum) {
    if (current_process != -1) return;
    
    int next_process = remove_from_ready_queue();
    if (next_process == -1) {
        // Instead of just incrementing by 1, we'll handle idle time in the main event loop
        #ifdef VERBOSE
        // printf("%d : CPU goes idle\n", current_time);
        #endif
        return;
    }
    
    current_process = next_process;
    PCB *p = &processes[current_process];
    p->state = RUNNING;
    
    int burst_time;
    if (p->remaining_time > 0) {
        burst_time = p->remaining_time;
    } else {
        burst_time = p->cpu_bursts[p->current_burst];
        p->remaining_time = burst_time;
    }
    
    if (quantum != INT_MAX && burst_time > quantum) {
        // Process will timeout
        Event e = {current_time + quantum, current_process, CPU_QUANTUM_TIMEOUT};
        add_to_event_queue(e);
        p->remaining_time = burst_time - quantum;
    } else {
        // Process will complete its CPU burst
        Event e = {current_time + burst_time, current_process, CPU_BURST_COMPLETION};
        add_to_event_queue(e);
        p->remaining_time = 0;
    }
    
    #ifdef VERBOSE
    printf("%d : Process %d is scheduled to run for time %d\n", 
           current_time, p->id, (quantum != INT_MAX && burst_time > quantum) ? quantum : burst_time);
    #endif
}

void handle_event(Event event, int quantum) {
    current_time = event.time;
    PCB *p = &processes[event.process_id];
    
    switch (event.type) {
        case PROCESS_ARRIVAL:
            #ifdef VERBOSE
            printf("%d : Process %d joins ready queue upon arrival\n", 
                   current_time, p->id);
            #endif
            p->state = READY;
            add_to_ready_queue(event.process_id);
            break;
            
        case CPU_BURST_COMPLETION:
            if (p->current_burst == p->num_bursts - 1) {
                // Process completed
                p->state = TERMINATED;
                p->turnaround_time = current_time - p->arrival_time;
                // Calculate wait time as turnaround time - total execution time
                p->wait_time = p->turnaround_time - p->total_execution_time;
                float percent = (float)p->turnaround_time * 100 / p->total_execution_time;
                printf("%d : Process %d exits. Turnaround time = %d (%d%%), Wait time = %d\n",
                       current_time, p->id, p->turnaround_time, (int)percent, p->wait_time);
            } else {
                // Start IO burst
                p->state = WAITING;
                p->current_burst++;
                Event e = {current_time + p->io_bursts[p->current_burst - 1], 
                          event.process_id, IO_BURST_COMPLETION};
                add_to_event_queue(e);
            }
            current_process = -1;
            #ifdef VERBOSE
            printf("%d : CPU goes idle\n", current_time);
            #endif
            break;
            
        case IO_BURST_COMPLETION:
            #ifdef VERBOSE
            printf("%d : Process %d joins ready queue after IO completion\n", 
                   current_time, p->id);
            #endif
            p->state = READY;
            add_to_ready_queue(event.process_id);
            break;
            
        case CPU_QUANTUM_TIMEOUT:
            #ifdef VERBOSE
            printf("%d : Process %d joins ready queue after timeout\n", 
                   current_time, p->id);
            #endif
            p->state = READY;
            add_to_ready_queue(event.process_id);
            current_process = -1;
            break;
    }
    
    schedule_process(quantum);
}

void run_scheduler(int quantum) {
    int last_event_time = 0;  // Track the time of the last event
    
    while (event_queue_size > 0) {
        Event event = remove_from_event_queue();
        
        // If CPU was idle and no process was running, add the gap to idle time
        if (current_process == -1 && event.time > last_event_time) {
            cpu_idle_time += event.time - last_event_time;
        }
        
        // Update last event time before handling the new event
        last_event_time = event.time;
        
        handle_event(event, quantum);
    }
    
    // Print final statistics
    float total_wait_time = 0;
    for (int i = 0; i < num_processes; i++) {
        total_wait_time += processes[i].wait_time;
    }
    
    printf("Average wait time = %.2f\n", total_wait_time / num_processes);
    printf("Total turnaround time = %d\n", current_time);
    printf("CPU idle time = %d\n", cpu_idle_time);
    float cpu_util = 100.0 * (current_time - cpu_idle_time) / current_time;
    printf("CPU utilization = %.2f%%\n", cpu_util);
}

int main() {
    read_processes("input.txt");
    
    printf("**** FCFS Scheduling ****\n");
    init_scheduler(INT_MAX);
    run_scheduler(INT_MAX);
    
    printf("\n**** RR Scheduling with q = 10 ****\n");
    init_scheduler(10);
    run_scheduler(10);
    
    printf("\n**** RR Scheduling with q = 5 ****\n");
    init_scheduler(5);
    run_scheduler(5);
    
    return 0;
}