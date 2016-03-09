
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd = -1;
    char buf[1000];
    fprintf(stdout, "hi\n");


    // fd = open("/dev/urandom", O_RDWR);
    fd = open("/dev/random", O_RDWR);

    fprintf(stdout, "fd=%d\n", fd);

    int n = read(fd, buf, 1000); 

    fprintf(stdout, "n=%d\n", n);
    
    return 0;
}



