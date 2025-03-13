#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "boardgen.c"

#define BLOCK_COUNT 9
#define BLOCK_SIZE 3
#define BOARD_SIZE 9
#define BUFFER_SIZE 256

typedef struct {
    int read_fd;
    int write_fd;
} PipePair;

PipePair pipes[BLOCK_COUNT];

void get_neighbors(int block, int* row_neighbors, int* col_neighbors) {
    int row = block / 3;
    int col = block % 3;
    
    row_neighbors[0] = (col > 0) ? block - 1 : -1;
    row_neighbors[1] = (col < 2) ? block + 1 : -1;
    
    col_neighbors[0] = (row > 0) ? block - 3 : -1;
    col_neighbors[1] = (row < 2) ? block + 3 : -1;
}

void create_xterm_command(char* cmd, int block_num, int read_fd, int write_fd, 
                         int rn1_fd, int rn2_fd, int cn1_fd, int cn2_fd) {
    int x = 200 + (block_num % 3) * 200;
    int y = 100 + (block_num / 3) * 150;
    
    sprintf(cmd, "xterm -T \"Block %d\" -fa Monospace -fs 15 -geometry \"17x8+%d+%d\" "
            "-bg \"#331100\" -e ./block %d %d %d %d %d %d %d",
            block_num, x, y, block_num, read_fd, write_fd, rn1_fd, rn2_fd, cn1_fd, cn2_fd);
}

void write_str(int fd, const char* str) {
    // printf("%s,%d", str,strlen(str));
    // fflush(stdout);
    write(fd, str, strlen(str));
    return;
}

void print_help() {
    write_str(STDOUT_FILENO, "\nFoodoku Commands:\n");
    write_str(STDOUT_FILENO, "h - Show this help message\n");
    write_str(STDOUT_FILENO, "n - Start new game\n");
    write_str(STDOUT_FILENO, "p b c d - Place digit d in cell c of block b\n");
    write_str(STDOUT_FILENO, "s - Show solution\n");
    write_str(STDOUT_FILENO, "q - Quit game\n");
    write_str(STDOUT_FILENO, "\nBlocks and cells are numbered 0-8 in row-major order\n");
}

void send_board_to_block(int block, int write_fd, int board[BOARD_SIZE][BOARD_SIZE]) {
    char buffer[BUFFER_SIZE];
    int row_start = (block / 3) * 3;
    int col_start = (block % 3) * 3;
    
    write_str(write_fd, "n ");
    
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            snprintf(buffer, BUFFER_SIZE, "%d ", board[row_start + i][col_start + j]);
            write_str(write_fd, buffer);
        }
    }
    write_str(write_fd, "\n");
}

int read_command(char* command, int* args, int max_args) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) return 0;
    
    buffer[bytes_read] = '\0';
    *command = buffer[0];
    
    if (max_args > 0) {
        char* ptr = buffer + 1;
        for (int i = 0; i < max_args; i++) {
            while (*ptr == ' ') ptr++;
            if (*ptr == '\0' || *ptr == '\n') break;
            args[i] = atoi(ptr);
            while (*ptr != ' ' && *ptr != '\n' && *ptr != '\0') ptr++;
        }
    }
    
    return 1;
}

int main() {
    int board[BOARD_SIZE][BOARD_SIZE];
    int solution[BOARD_SIZE][BOARD_SIZE];
    char command;
    int args[3];
    
    // Create pipes
    for (int i = 0; i < BLOCK_COUNT; i++) {
        int fds[2];
        if (pipe(fds) == -1) {
            perror("pipe creation failed");
            exit(1);
        }
        pipes[i].read_fd = fds[0];
        pipes[i].write_fd = fds[1];
    }
    
    // Fork child processes
    for (int block = 0; block < BLOCK_COUNT; block++) {
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork failed");
            exit(1);
        }
        
        if (pid == 0) {  // Child process
            int row_neighbors[2], col_neighbors[2];
            get_neighbors(block, row_neighbors, col_neighbors);

                        for (int i = 0; i < BLOCK_COUNT; i++) {
                if (i != block) {
                    close(pipes[i].read_fd);
                }
                if (!((i == block) || 
                      (i == row_neighbors[0]) || 
                      (i == row_neighbors[1]) || 
                      (i == col_neighbors[0]) || 
                      (i == col_neighbors[1]))) {
                    close(pipes[i].write_fd);
                }
                //  close(pipes[i].write_fd);
            }
            

            char cmd[512];
            create_xterm_command(cmd, block, 
                               pipes[block].read_fd, 
                               pipes[block].write_fd,
                               row_neighbors[0] >= 0 ? pipes[row_neighbors[0]].write_fd : -1,
                               row_neighbors[1] >= 0 ? pipes[row_neighbors[1]].write_fd : -1,
                               col_neighbors[0] >= 0 ? pipes[col_neighbors[0]].write_fd : -1,
                               col_neighbors[1] >= 0 ? pipes[col_neighbors[1]].write_fd : -1);
            
            execl("/bin/sh", "sh", "-c", cmd, NULL);
            // system(cmd);
                        // Close unused pipe ends

            
            perror("exec failed");
            exit(1);
        }
    }
    
    // Parent process (coordinator)
    while (1) {
        write_str(STDOUT_FILENO, "\nEnter command: ");
        // printf("\n");
        if (!read_command(&command, args, 3)) continue;
        
        switch (command) {
            case 'h':
                print_help();
                break;
                
            case 'n': {
                newboard(board, solution);
                for (int block = 0; block < BLOCK_COUNT; block++) {
                    send_board_to_block(block, pipes[block].write_fd, board);
                }
                break;
            }
                
            case 'p': {
                int b = args[0], c = args[1], d = args[2];
                
                if (b < 0 || b >= BLOCK_COUNT || c < 0 || c >= BLOCK_COUNT || 
                    d < 1 || d > BLOCK_COUNT) {
                    write_str(STDOUT_FILENO, "Invalid input ranges\n");
                    break;
                }
                
                char buffer[BUFFER_SIZE];
                snprintf(buffer, BUFFER_SIZE, "p %d %d\n", c, d);
                // fflush(stdout);
                write_str(pipes[b].write_fd, buffer);

                break;
            }
                
            case 's': {
                for (int block = 0; block < BLOCK_COUNT; block++) {
                    send_board_to_block(block, pipes[block].write_fd, solution);
                }
                break;
            }
                
            case 'q':
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    write_str(pipes[i].write_fd, "q\n");
                }
                
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    wait(NULL);
                }
                exit(0);
                
            default:
                write_str(STDOUT_FILENO, "Invalid command. Use 'h' for help.\n");
        }
    }
    
    return 0;
}