#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#define DEV   "/dev/ezdma"
#define BUFSZ 4096
#define ITER  100000

static long long elapsed_ns(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000000000LL +
           (b->tv_nsec - a->tv_nsec);
}

int main(void) {
    int fd;
    char user_buf[BUFSZ];
    void *map;
    struct timespec t0, t1;
    long long ns_read, ns_mmap;

    printf("=== Benchmark Zero-Copy vs Copia Tradicional ===\n");
    printf("Iteracoes: %d | Buffer: %d bytes\n\n", ITER, BUFSZ);

    fd = open(DEV, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITER; i++) {
        lseek(fd, 0, SEEK_SET);
        if (read(fd, user_buf, BUFSZ) < 0) { perror("read"); return 1; }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ns_read = elapsed_ns(&t0, &t1);
    close(fd);
    printf("[read()]  total: %lld ns | media: %lld ns/op\n",
           ns_read, ns_read / ITER);

    fd = open(DEV, O_RDONLY);
    map = mmap(NULL, BUFSZ, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITER; i++) memcpy(user_buf, map, BUFSZ);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ns_mmap = elapsed_ns(&t0, &t1);
    munmap(map, BUFSZ);
    close(fd);
    printf("[mmap()]  total: %lld ns | media: %lld ns/op\n",
           ns_mmap, ns_mmap / ITER);

    printf("\n=== Resultado ===\n");
    if (ns_mmap < ns_read) {
        double ganho = ((double)(ns_read - ns_mmap) / ns_read) * 100.0;
        printf("mmap() foi %.2f%% mais rapido | Speedup: %.2fx\n",
               ganho, (double)ns_read / ns_mmap);
    }
    return 0;
}
