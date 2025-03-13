#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BLOCK_SIZE 3

typedef struct {
    int original[BLOCK_SIZE][BLOCK_SIZE];  // Original puzzle state
    int current[BLOCK_SIZE][BLOCK_SIZE];   // Current state
} BlockState;

void draw_block(BlockState* state) {
    printf("\033[H\033[J");  // Clear screen
    // fflush(stdout);
    printf("┌───┬───┬───┐\n");
    // fflush(stdout);
    
    for (int i = 0; i < BLOCK_SIZE; i++) {
        printf("│");
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (state->current[i][j] == 0) {
                printf(" · ");
            } else {
                printf(" %d ", state->current[i][j]);
            }
            printf("│");
        }
        printf("\n");
        
        if (i < BLOCK_SIZE - 1) {
            printf("├───┼───┼───┤\n");
        }
        // fflush(stdout);
    }
    
    printf("└───┴───┴───┘\n");
    // fflush(stdout);
}

void print_error(const char* message) {
    printf("\n%s\n", message);
    fflush(stdout);
    sleep(2);  // Keep error message visible for 2 seconds
}

int check_row_conflict(int row, int digit, BlockState* state) {
    for (int j = 0; j < BLOCK_SIZE; j++) {
        if (state->current[row][j] == digit) {
            return 1;
        }
    }
    return 0;
}

int check_col_conflict(int col, int digit, BlockState* state) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (state->current[i][col] == digit) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Usage: %s block_num read_fd write_fd rn1_fd rn2_fd cn1_fd cn2_fd\n", 
                argv[0]);
        exit(1);
    }
    
    int block_num = atoi(argv[1]);
    int read_fd = atoi(argv[2]);
    int write_fd = atoi(argv[3]);
    int row_neighbor_fds[2] = {atoi(argv[4]), atoi(argv[5])};
    int col_neighbor_fds[2] = {atoi(argv[6]), atoi(argv[7])};
    
    BlockState state = {0};
    char command;
    
    // Create FILE* for reading and writing
    FILE* read_stream = fdopen(read_fd, "r");
    FILE* write_stream = fdopen(write_fd, "w");
    if (!read_stream || !write_stream) {
        perror("fdopen failed");
        exit(1);
    }
    
    // Create FILE* for neighbors
    FILE* row_streams[2] = {NULL, NULL};
    FILE* col_streams[2] = {NULL, NULL};
    
    for (int i = 0; i < 2; i++) {
        if (row_neighbor_fds[i] != -1) {
            row_streams[i] = fdopen(row_neighbor_fds[i], "w");
            if (!row_streams[i]) {
                perror("fdopen failed for row neighbor");
                exit(1);
            }
        }
        if (col_neighbor_fds[i] != -1) {
            col_streams[i] = fdopen(col_neighbor_fds[i], "w");
            if (!col_streams[i]) {
                perror("fdopen failed for col neighbor");
                exit(1);
            }
        }
    }
    
    while (1) {
        do {
            fscanf(read_stream, "%c", &command);
        }
        while (command == '\n' || command == ' ' || command == '\t' || command == '\r' || command == '\f' || command == '\v' || command == '\0' || command == '\a' || command == '\b');
        
        switch (command) {
            case 'n': {
                // Receive new board state
                for (int i = 0; i < BLOCK_SIZE; i++) {
                    for (int j = 0; j < BLOCK_SIZE; j++) {
                        if (fscanf(read_stream, "%d", &state.original[i][j]) != 1) {
                            fprintf(stderr, "Failed to read board state\n");
                            exit(1);
                        }
                        state.current[i][j] = state.original[i][j];
                    }
                }
                draw_block(&state);
                break;
            }
                
            case 'p': {
                int cell, digit;
                if (fscanf(read_stream, "%d %d", &cell, &digit) != 2) {
                    fprintf(stderr, "Failed to read cell and digit\n");
                    break;
                }
                
                int row = cell / BLOCK_SIZE;
                int col = cell % BLOCK_SIZE;
                
                // Check if cell is read-only
                if (state.original[row][col] != 0) {
                    print_error("Error: Read-only cell");
                    draw_block(&state);
                    break;
                }
                
                // Check block conflict
                if (check_row_conflict(row, digit, &state) || 
                    check_col_conflict(col, digit, &state)) {
                    print_error("Error: Block conflict");
                    draw_block(&state);
                    break;
                }
                
                // Check row neighbors
                int has_conflict = 0;
                for (int i = 0; i < 2 && !has_conflict; i++) {
                    if (row_streams[i]) {
                        fprintf(row_streams[i], "r %d %d\n", row, digit);
                        fflush(row_streams[i]);
                        
                        int response;
                        if (fscanf(read_stream, "%d", &response) != 1) {
                            fprintf(stderr, "Failed to read neighbor response\n");
                            break;
                        }
                        if (response != 0) {
                            has_conflict = 1;
                            print_error("Error: Row conflict");
                        }
                    }
                }
                
                if (has_conflict) {
                    draw_block(&state);
                    break;
                }
                
                // Check column neighbors
                for (int i = 0; i < 2 && !has_conflict; i++) {
                    if (col_streams[i]) {
                        fprintf(col_streams[i], "c %d %d\n", col, digit);
                        fflush(col_streams[i]);
                        
                        int response;
                        if (fscanf(read_stream, "%d", &response) != 1) {
                            fprintf(stderr, "Failed to read neighbor response\n");
                            break;
                        }
                        if (response != 0) {
                            has_conflict = 1;
                            print_error("Error: Column conflict");
                        }
                    }
                }
                
                if (has_conflict) {
                    draw_block(&state);
                    break;
                }else{
                    state.current[row][col] = digit;
                    draw_block(&state);
                    break;
                }
                
                // Update cell if no conflicts
                // draw_block(&state);
                
            }
                
            case 'r': {
                int row, digit;
                if (fscanf(read_stream, "%d %d", &row, &digit) != 2) {
                    fprintf(stderr, "Failed to read row check parameters\n");
                    break;
                }
                int conflict = check_row_conflict(row, digit, &state);
                fprintf(write_stream, "%d\n", conflict);
                fflush(write_stream);
                break;
            }
                
            case 'c': {
                int col, digit;
                if (fscanf(read_stream, "%d %d", &col, &digit) != 2) {
                    fprintf(stderr, "Failed to read column check parameters\n");
                    break;
                }
                int conflict = check_col_conflict(col, digit, &state);
                fprintf(write_stream, "%d\n", conflict);
                fflush(write_stream);
                break;
            }
                
            case 'q':
                printf("\nBlock %d says bye...\n", block_num);
                fflush(stdout);
                sleep(1);
                exit(0);
                
            default:
                // fprintf(stderr, "Unknown command: %c\n", command);
        }
        fflush(stdout);
    }
    
    return 0;
}