#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h> /* mlockall */
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    if (mlockall(MCL_CURRENT | MCL_FUTURE))
        perror("mlockall failed:");

    for (int i = 0; i <= offset; i++) {
        long long time_m1, time_m2, time_m3;
        lseek(fd, i, SEEK_SET);
        time_m1 = write(fd, write_buf, 1); /* origin iterative method*/
        time_m2 = write(fd, write_buf, 2); /* fast doubling */
        time_m3 = write(fd, write_buf, 3); /* fast doubling-with ctz */

        printf("%lld %lld %lld\n", time_m1, time_m2, time_m3);
    }

    close(fd);
    return 0;
}