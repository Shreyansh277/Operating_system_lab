#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

int noChild;           // no of childern
pid_t *cpids;          // array to store child process ids
int *current_status;   // array to store the status of childern   0 for not out 1 for out
int signalRecived = 0; // flag to check if signal is received or not
int catch_result = 0;  // flag to check if the ball is catched or not
int currentPlayer = 0; // current player to pass the ball
pid_t dpid;            // dummy process id

// print no on the screen
void printChildern()
{
    for (int i = 1; i <= noChild; i++)
    {
        printf("   %d   ", i);
        fflush(stdout);
    }
    printf("\n");
    fflush(stdout);
    return;
}

// child creation and writing to file
void childCreationAndWritefiles()
{
    FILE *file = fopen("child.txt", "w");
    if (file == NULL)
    {
        printf("Error in opening file\n");
        exit(1);
    }
    fprintf(file, "%d\n", noChild);
    for (int i = 0; i < noChild; i++)
    {
        int pid = fork();
        if (pid == 0)
        { // repalcing child process with child.c
            char str[100];
            sprintf(str, "%d", i);
            execl("./child", "./child", str, NULL);
            perror("execl faliled");
            exit(1);
        }
        else if (pid > 0)
        {
            // writing child process id to the file
            fprintf(file, "pid of child%d is %d\n", i + 1, pid);
            cpids[i] = pid;
        }
        else
        {
            perror("fork failed");
            exit(1);
        }
    }
    fclose(file);

    printf("Parent: %d child processes created\n", noChild);
    printf("Parent: Waiting for child processes to read child database\n");
    printChildern();
    return;
}

// signal handler to handel the signals
void signalHandler(int signum)
{
    // catch the ball
    if (signum == SIGUSR1)
    {
        signalRecived = 1;
        catch_result = 1;
    }
    // if the ball is missed
    else if (signum == SIGUSR2)
    {
        signalRecived = 1;
        catch_result = 0;
    }
    // printf("signal received\n");

    return;
}

// function to count the no of playing childern
int countPlayingChildern()
{
    int count = 0;
    for (int i = 0; i < noChild; i++)
    {
        if (current_status[i] == 0)
        {
            count++;
        }
    }
    return count;
}
// function to get the next player
int nextPlayer(int currentPlayer)
{
    int nextPlayer = (currentPlayer + 1) % noChild;
    while (current_status[nextPlayer] == 1)
    {
        nextPlayer = (nextPlayer + 1) % noChild;
        if (nextPlayer == currentPlayer)
        {
            break;
        }
    }
    return nextPlayer;
}
// function to create dummy process and write the pid to the file
void createDummyProcess()
{
    dpid = fork();
    if (dpid == 0)
    {
        execl("./dummy", "./dummy", NULL);
        perror("execl failed");
        exit(1);
    }
    else if (dpid > 0)
    {
        FILE *dfile = fopen("dummycpid.txt", "w");
        fprintf(dfile, "%d\n", dpid);
        fclose(dfile);
    }
    else
    {
        perror("fork failed");
        exit(1);
    }
    return;
}

// function to throw the ball to the player
void throwBall(int player)
{
    signalRecived = 0;
    kill(cpids[player], SIGUSR2);
    if (signalRecived == 0)pause();
    // pause();
    signalRecived = 0;
    if (catch_result == 0)
    {
        current_status[player] = 1;
    }
}

void gameplay()
{
    currentPlayer = 0;
    while (countPlayingChildern() > 1)
    {
        createDummyProcess();
        // printf("created dummy process\n");
        fflush(stdout);
        throwBall(currentPlayer);
        // printf("parent received signal\n");
        fflush(stdout);
        currentPlayer = nextPlayer(currentPlayer);
        kill(cpids[0], SIGUSR1);
        waitpid(dpid, NULL, 0);
    }
    return;
}

int main(int argc, char *argv[])
{ // if no of childern is not provided as argument
    if (argc < 2)
    {
        printf("Usage: %s <noChild>\n", argv[0]);
        exit(1);
    }
    noChild = atoi(argv[1]); // no of childern

    // handling signals

    signal(SIGUSR1, signalHandler);
    signal(SIGUSR2, signalHandler);

    cpids = (pid_t *)malloc(noChild * sizeof(pid_t));
    current_status = (int *)calloc(noChild, sizeof(int));

    // creating childern and writing pids to the files
    childCreationAndWritefiles();

    // sleep for 2 second to let the child to read the file
    sleep(2);

    gameplay();
    printChildern();

    // GAME OVER SENDING SIGNAL TO ALL THE CHILDERN
    for (int i = 0; i < noChild; i++)
    {
        kill(cpids[i], SIGINT);
    }

    for (int i = 0; i < noChild; i++)
    {
        wait(NULL);
    }

    free(cpids);
    free(current_status);

    exit(0);
}
