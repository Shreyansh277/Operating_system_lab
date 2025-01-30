#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include<time.h>

#define MISS 1
#define CATCH 2
#define PLAYING 3
#define OUT 4

int indexNo;                                          // indexNo of the child
int noChild;                                        // no of childern
pid_t* cpids;                                     // array to store child process ids
int gameStatus=3;                                     // flag to check if the game is over or not


void signalHandler(int signo)
{
    if(signo == SIGUSR2)
    {
        double t = (double)rand()/RAND_MAX;
        // catch is missed
        if(t>=0.80)
        { 
          gameStatus = MISS;
          kill(getppid(), SIGUSR2);
        }
        // catch is successfull
        else
        {   gameStatus = CATCH;
            kill(getppid(), SIGUSR1);
        }
    }
    else if(signo == SIGUSR1)
    {
        if(indexNo==0){
        printf("+"); fflush(stdout);
        for(int i=0;i<noChild;i++){printf("-------"); fflush(stdout);}
        printf("+\n"); fflush(stdout);
       printf("|");
       fflush(stdout);
        }

       if(gameStatus==OUT)printf("       ");
       else if(gameStatus==MISS){printf("  MISS "); gameStatus=OUT;}
       else if(gameStatus==CATCH) {printf(" CATCH "); gameStatus=PLAYING;}
       else printf(" ..... ");
       fflush(stdout);

       if(indexNo!=noChild-1)kill(cpids[indexNo+1], SIGUSR1);
       else 
       {
        printf("|\n"); fflush(stdout);
                 printf("+"); fflush(stdout);
        for(int i=0;i<noChild;i++){printf("-------"); fflush(stdout);}
        printf("+\n"); fflush(stdout);

         FILE* fp = fopen("dummycpid.txt", "r");
         int dpid;
         fscanf(fp, "%d", &dpid);
         fclose(fp);
         kill(dpid, SIGINT);
       }


    }
    else if(signo == SIGINT)
    {
        if(gameStatus == PLAYING)
        {
            // this child is the winner
            printf("+++Child %d: Yay! I am the winner\n",indexNo+1);
            fflush(stdout);
        }
        exit(0);
  
    }  

fflush(stdout);

  return;
}


void readfile()
{
    
    FILE* file = fopen("child.txt", "r");
    char buffer[100];
    fgets(buffer, 100, file);
    noChild= atoi(buffer);
    cpids = (pid_t*)malloc(noChild*sizeof(pid_t));
    int t;
    for(int i=0;i<noChild;i++)
    {
        fgets(buffer, 100, file);
        sscanf(buffer, "pid of child%d is %d\n", &t, &cpids[i]);
    }
    fclose(file);
    return;
    }

int main(int argc, char *argv[])
{
    srand(time(NULL)^getpid());
    // argument check
    if(argc < 2)
    {
        printf("Usage: %s <indexNo> <noChild>\n", argv[0]);
        exit(1);
    }
    
    // indexNo of the child
    indexNo = atoi(argv[1]);
    sleep(1);
        
    // reading the file
    readfile();
    
    // handling signals
    signal(SIGUSR1, signalHandler);
    signal(SIGUSR2, signalHandler);
    signal(SIGINT, signalHandler);

    // waits for signals 
    while(1)pause();
    free(cpids);
    exit(0);
}





