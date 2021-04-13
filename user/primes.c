#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDDER_FILENO 2

#define READEND 0
#define WRITEEND 1
typedef int pid_t;

int main() {
    pid_t pid; 
    int fd[2];
    int number[36];
    int index = 0;

    for(int i = 2; i <= 35; i++) {
        number[index] = i;
        index += 1;
    }
    while(index > 0) {
        pipe(fd);
        if ((pid = fork()) < 0) {
            fprintf(STDDER_FILENO, "fork error\n");
            exit(0);
        }

        if(pid > 0){
            close(fd[READEND]);
            for( int i = 0; i < index; i++) {
                write(fd[WRITEEND], &number[i], sizeof(number[i]));
            }
            close(fd[WRITEEND]);
            wait((int *)0);

            exit(0);
        }else if( pid == 0 ) {
            close(fd[WRITEEND]);
            int num;
            index = 0;
            int p = -1; 
            while( read(fd[READEND], &num, sizeof(num)) != 0) {
                if (p == -1) {
                    printf("prime %d\n", num);
                    p = num;
                }else {
                    if (num % p != 0) {
                        number[index] = num;
                        index += 1;
                    }
                }
            }
            close(fd[READEND]);
            //continue to write 
        }
    }

    exit(0);
}