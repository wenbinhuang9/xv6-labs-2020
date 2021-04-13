#include <stdio.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDDER_FILENO 2

int readline(char* buf, int buflenth) {
    char c; 
    int i = 0; 
    while (read(STDIN_FILENO, &c, sizeof(char)) > 0) {
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
int main() {
    char buf[1024];
    write(STDIN_FILENO, "1\n2", 3);
    while (readline(buf, 1024) > 0) 
    {
        printf("%s\n");    
    }

    return 0;
    
}