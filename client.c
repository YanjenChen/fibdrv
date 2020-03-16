#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

#include "bign.h"

int main()
{
    long long sz;

    struct bign128 buf[1];
    char write_buf[] = "testing writing";
    FILE *time_log;
    struct timespec t_start, t_end;
    int offset = 100; /* TODO: try test something bigger than the limit */

    time_log = fopen("time.log", "w");
    if (!time_log) {
        perror("Failed to open log file");
        exit(1);
    }

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_REALTIME, &t_start);
        sz = read(fd, buf, 1);
        clock_gettime(CLOCK_REALTIME, &t_end);
        printf("Reading from " FIB_DEV " at offset %d, returned the sequence ",
               i);
        if (buf[0].upper == 0)
            printf("%llx.\n", buf[0].lower);
        else
            printf("%llx%llx.\n", buf[0].upper, buf[0].lower);
        /* Log execution time */
        fprintf(time_log, "%d %lld %ld %lld\n", i, sz,
                t_end.tv_nsec - t_start.tv_nsec,
                t_end.tv_nsec - t_start.tv_nsec - sz);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV " at offset %d, returned the sequence ",
               i);
        if (buf[0].upper == 0)
            printf("%llx.\n", buf[0].lower);
        else
            printf("%llx%llx.\n", buf[0].upper, buf[0].lower);
    }

    fclose(time_log);
    close(fd);
    return 0;
}
