#include<bits/stdc++.h>
#include <sys/wait.h>
using namespace std;
#define NULL __null
#define Max_Foodules 100
#define maxDependencies 100

int visited[Max_Foodules];


void initialiseVisited(int n)
{
    ofstream file("done.txt");
    if(!file)
    {
        cout<<"Error opening done.txt\n";
        exit(1);
    }
    for(int i=0;i<n;i++)
    {
        file<<"0";
    }
    file.close();
}

void readVisited(int visited[],int n)
{
    ifstream file("done.txt");
    if(!file)
    {
        cout<<"Error opening done.txt\n";
        exit(1);
    }
    for(int i=0;i<n;i++)
    {
        visited[i]=file.get()-'0';
        // cout<<visited[i];
    }
    file.close();
}

void readDependencies(int foodule,int dependencies[],int &depCount)
{
    int n;
    ifstream file("foodep.txt");
    if(!file)
    {
        cout<<"Error opening foodep.txt\n";
        exit(1);
    }
    // file>>n;
    


    char line[1024];
    file.getline(line,sizeof(line));
    n=atoi(line);
    for(int i=1;i<=foodule;i++)
    {
        if(!file.getline(line,sizeof(line)))
        {
            break;
        }
        if(i==foodule)
        {
            depCount=0;
            char *token=strtok(line,": ");
            token=strtok(NULL,": ");
            while(token!=NULL)
            {
                // cout<<token<<'\n';
                dependencies[depCount++]=atoi(token);
                token=strtok(NULL," ");
            }
            break;
        }
    }
    file.close();
}

void writeVisited(int visited[],int n)
{
    ofstream file("done.txt");
    if(!file)
    {
        cout<<"Error opening done.txt\n";
        exit(1);
    }
    for(int i=0;i<n;i++)
    {
        file<<visited[i];
    }
    file.close();
}


void rebuild(int foodule ,int is_root)
{
    int n; 
    int dependencies[maxDependencies];
    int depCount=0;
    ifstream file("foodep.txt");
    if(!file)
    {
        cout<<"Error opening foodep.txt\n";
        exit(1);
    }

    file>>n;
    file.close();
    
    if(is_root)
    {
       initialiseVisited(n);
    }
    

    readDependencies(foodule,dependencies,depCount);
    int visited[Max_Foodules];
    readVisited(visited,n);

    for(int i=0;i<depCount;i++)
    {
        
        readVisited(visited,n);
        int dep=dependencies[i];
        if(visited[dep-1]==0)
        {  
             visited[dep-1]=1;
            pid_t pid=fork();
            if(pid==0)
            {
                char dep_str[10],is_root_str[10];
                sprintf(dep_str,"%d",dep);
                sprintf(is_root_str,"%d",0);
                execl("./rebuild","./rebuild",dep_str,is_root_str,(char *)NULL);
            
                perror("Error in exec");
                exit(1);
            }
            else if(pid>0)
            {
                wait(NULL);
            }
            else
            {
                perror("Error in fork");
                exit(1);
            }
        }
    }

// upadate visited array
visited[foodule-1]=1;
writeVisited(visited,n);

// print rebuilding message 
if(depCount==0)cout<<"foo"<<foodule<<" rebuilt "<<endl;

if(depCount>0)
{
    cout<<"foo"<<foodule<<" rebuilt "<<"from";
    for(int i=0;i<depCount;i++)
    {
      cout<<" foo"<<dependencies[i]<<" ";
    }cout<<endl;
}

return;
}



int main(int argc,char *argv[])
{
    if((argc<2))
    {
     cout<<"Usage: "<<argv[0]<<" <foodule> [is_root]\n";
     exit(1);
    }

    int foodule =atoi(argv[1]);
    int is_root=(argc==2); // if is_root is not given then it is not root
    rebuild(foodule,is_root);
    return 0;
}
