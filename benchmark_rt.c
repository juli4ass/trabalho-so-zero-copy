/*
 * benchmark_rt.c - Benchmark Real-Time para o driver ezdma
 *
 * Aplica tecnicas reais de Sistemas de Tempo Real:
 *   - mlockall(): trava paginas em RAM (evita page faults)
 *   - SCHED_FIFO prio 80: escalonamento de tempo real
 *   - CPU pinning: reduz migracao de tarefa entre CPUs
 *   - Warm-up: descarta primeiras amostras (cache frio)
 *   - Coleta amostra-a-amostra para analise estatistica
 *   - Exporta CSV para gerar graficos
 *
 * Metricas: min, media, P50, P95, P99, P99.9, max (WCET), jitter
 *
 * Compilar: gcc -O2 -Wall -o benchmark_rt benchmark_rt.c -lm
 * Executar: sudo ./benchmark_rt
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sched.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#define DEV     "/dev/ezdma"
#define BUFSZ   4096
#define WARMUP  1000
#define SAMPLES 100000
#define CPU_PIN 1
#define RT_PRIO 80

static long long ts_to_ns(struct timespec *t) {
    return t->tv_sec * 1000000000LL + t->tv_nsec;
}

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long*)a, y = *(const long long*)b;
    return (x > y) - (x < y);
}

static int setup_realtime(void) {
    struct sched_param sp;
    cpu_set_t mask;

    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
        perror("mlockall (rode com sudo)");
        return -1;
    }
    sp.sched_priority = RT_PRIO;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) < 0) {
        perror("sched_setscheduler (rode com sudo)");
        return -1;
    }
    CPU_ZERO(&mask);
    CPU_SET(CPU_PIN, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
        perror("sched_setaffinity");
        return -1;
    }
    return 0;
}

static long long percentile(long long *sorted, int n, double p) {
    int idx = (int)((p / 100.0) * (n - 1));
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    return sorted[idx];
}

static void analyze(const char *name, long long *samples, int n,
                    const char *csv_file) {
    long long sum = 0, mn = samples[0], mx = samples[0];
    double mean, variance = 0, stddev;
    long long *sorted = malloc(n * sizeof(long long));
    FILE *f;

    memcpy(sorted, samples, n * sizeof(long long));
    qsort(sorted, n, sizeof(long long), cmp_ll);

    for (int i = 0; i < n; i++) {
        sum += samples[i];
        if (samples[i] < mn) mn = samples[i];
        if (samples[i] > mx) mx = samples[i];
    }
    mean = (double)sum / n;
    for (int i = 0; i < n; i++) {
        double d = samples[i] - mean;
        variance += d * d;
    }
    variance /= n;
    stddev = sqrt(variance);

    printf("\n=== Resultados [%s] ===\n", name);
    printf("  Amostras   : %d\n", n);
    printf("  Min        : %lld ns\n", mn);
    printf("  Media      : %.2f ns\n", mean);
    printf("  Mediana P50: %lld ns\n", percentile(sorted, n, 50));
    printf("  P95        : %lld ns\n", percentile(sorted, n, 95));
    printf("  P99        : %lld ns\n", percentile(sorted, n, 99));
    printf("  P99.9      : %lld ns\n", percentile(sorted, n, 99.9));
    printf("  Max (WCET) : %lld ns\n", mx);
    printf("  Jitter (sd): %.2f ns\n", stddev);

    f = fopen(csv_file, "w");
    if (f) {
        fprintf(f, "sample_ns\n");
        for (int i = 0; i < n; i++) fprintf(f, "%lld\n", samples[i]);
        fclose(f);
        printf("  CSV salvo  : %s\n", csv_file);
    }
    free(sorted);
}

int main(void) {
    int fd, i;
    char user_buf[BUFSZ];
    void *map;
    struct timespec t0, t1;
    long long *samples_read, *samples_mmap;

    printf("=== Benchmark Real-Time: read() vs mmap() ===\n");
    printf("Amostras: %d (warmup: %d) | CPU pin: %d | Prio FIFO: %d\n",
           SAMPLES, WARMUP, CPU_PIN, RT_PRIO);

    if (setup_realtime() < 0) {
        fprintf(stderr, "Falha modo tempo real. Saindo.\n");
        return 1;
    }
    printf("Modo tempo real ATIVO (mlockall + SCHED_FIFO + CPU pin)\n");

    samples_read = malloc(SAMPLES * sizeof(long long));
    samples_mmap = malloc(SAMPLES * sizeof(long long));
    if (!samples_read || !samples_mmap) { perror("malloc"); return 1; }

    /* === read() === */
    fd = open(DEV, O_RDONLY);
    if (fd < 0) {
        perror("open /dev/ezdma");
        fprintf(stderr, "Driver carregado? sudo insmod ezdma_fake.ko\n");
        return 1;
    }
    for (i = 0; i < WARMUP; i++) {
        lseek(fd, 0, SEEK_SET);
        read(fd, user_buf, BUFSZ);
    }
    for (i = 0; i < SAMPLES; i++) {
        lseek(fd, 0, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        read(fd, user_buf, BUFSZ);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        samples_read[i] = ts_to_ns(&t1) - ts_to_ns(&t0);
    }
    close(fd);

    /* === mmap() === */
    fd = open(DEV, O_RDONLY);
    map = mmap(NULL, BUFSZ, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }

    for (i = 0; i < WARMUP; i++) memcpy(user_buf, map, BUFSZ);
    for (i = 0; i < SAMPLES; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        memcpy(user_buf, map, BUFSZ);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        samples_mmap[i] = ts_to_ns(&t1) - ts_to_ns(&t0);
    }
    munmap(map, BUFSZ);
    close(fd);

    analyze("read() tradicional", samples_read, SAMPLES, "results_read.csv");
    analyze("mmap() Zero-Copy",   samples_mmap, SAMPLES, "results_mmap.csv");

    {
        double m_r = 0, m_m = 0;
        long long max_r = 0, max_m = 0;
        for (i = 0; i < SAMPLES; i++) {
            m_r += samples_read[i]; m_m += samples_mmap[i];
            if (samples_read[i] > max_r) max_r = samples_read[i];
            if (samples_mmap[i] > max_m) max_m = samples_mmap[i];
        }
        m_r /= SAMPLES; m_m /= SAMPLES;
        printf("\n=== Comparativo (criterio tempo real) ===\n");
        printf("Speedup medio    : %.2fx\n", m_r / m_m);
        printf("Reducao do WCET  : %.2f%% (max %lld -> %lld ns)\n",
               (1.0 - (double)max_m / max_r) * 100.0, max_r, max_m);
    }

    free(samples_read); free(samples_mmap);
    return 0;
}
