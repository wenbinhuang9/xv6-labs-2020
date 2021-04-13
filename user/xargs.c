#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDDER_FILENO 2

typedef int pid_t;

int readline(char* buf, int buflenth) {
    char c; 
    int i = 0; 
    while (read(STDIN_FILENO, &c, sizeof(char)) > 0) {
        //printf("%c\n", c); 
        if (c == '\n') {
           
            break;
        }else {
            if (i >= buflenth) {
                fprintf(STDDER_FILENO, "buffer too small to read a line %d\n", buflenth);
                exit(1);
            }
            buf[i] = c;
            i++;
        }
    }
    buf[i] = '\0';
    return i; 
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("<usage>:%s -n 1 cmd\n");
        exit(1);
    }
    char buf[1024];
    char* newargv[MAXARG]; 
    
    int index = 0;
    for (int i = 1; i < argc; i++) {
        newargv[index] = argv[i];
        index += 1;
    }
    pid_t  pid;
    while(readline(buf, 1024) > 0) {
        //printf("buf is %s\n", buf);
        if((pid = fork()) < 0) {
            fprintf(STDDER_FILENO, "can not fork\n");
        }
        if (pid == 0) {
            newargv[index] = buf;
            index += 1;
            char* cmd = argv[1];

            exec(cmd, newargv);

        }else {
            wait((int*) 0);
        }
    }
    exit(0);
}