#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>


// signal handler to handel the signals
void signalHandler(int signum)
{
    if(signum == SIGINT)
    {
        exit(1);
    }
}

int main()
{
    // signal is recieved 
    signal(SIGINT, signalHandler);

    // pause the process until the signal is received
    while(1)pause();

    exit(0);
}