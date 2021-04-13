#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDDER_FILENO 2

#define READEND 0
#define WRITEEND 1

typedef int pid_t;

int main(void)
{
    pid_t c; 
    int pfd[2];
    int cfd[2];

    pipe(pfd);
    pipe(cfd);

    c = fork();
    
    if (c < 0) {
        fprintf(STDDER_FILENO, "fork error\n");
        exit(1);
    }
    if(c ==0) {
        char* buf[10];
        close(pfd[1]);
        close(cfd[0]);
        read(pfd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);

        write(cfd[1], "pong", 4);
    }else {
        char* buf[10];
        close(pfd[0]); 
        close(cfd[1]);

        write(pfd[1], "ping", 4);
        read(cfd[0], buf, 4);

        printf("%d: received %s\n", getpid(), buf);
    }

    exit(0);
}