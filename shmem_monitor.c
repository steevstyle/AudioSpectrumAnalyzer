#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#define PRU_SHARED_MEM 0x4A310000
#define PRU_MEM_SIZE 0x3000
#define BUFFER_SIZE 1024

volatile int keep_running = 1;

void signal_handler(int signum) {
    keep_running = 0;
}

void analyze_buffer(volatile uint16_t* buffer, const char* name) {
    uint16_t min = 4095, max = 0;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        uint16_t val = buffer[i];
        if (val < min) min = val;
        if (val > max) max = val;
        sum += val;
        sum_sq += val * val;
    }

    double avg = sum / BUFFER_SIZE;
    double variance = (sum_sq / BUFFER_SIZE) - (avg * avg);
    double std_dev = sqrt(variance);

    printf("%s:\n", name);
    printf("  Min: %4d (%.3fV)  Max: %4d (%.3fV)\n",
           min, min * 1.8 / 4095.0, max, max * 1.8 / 4095.0);
    printf("  Avg: %4.1f (%.3fV)  StdDev: %.1f (%.3fV)\n",
           avg, avg * 1.8 / 4095.0, std_dev, std_dev * 1.8 / 4095.0);

    // Show first 16 samples
    printf("  First 16 samples: ");
    for (int i = 0; i < 16; i++) {
        printf("%4d ", buffer[i]);
    }
    printf("\n");
}

int main() {
    int mem_fd;
    void *shared_map;
    volatile uint16_t *buffer_a, *buffer_b;
    volatile uint8_t *ready_flag;

    printf("Shared Memory Monitor\n");
    printf("=====================\n\n");

    signal(SIGINT, signal_handler);

    // Map shared memory
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Cannot open /dev/mem");
        return 1;
    }

    shared_map = mmap(0, PRU_MEM_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED, mem_fd, PRU_SHARED_MEM);
    if (shared_map == MAP_FAILED) {
        perror("Cannot map shared memory");
        close(mem_fd);
        return 1;
    }

    buffer_a = (volatile uint16_t *)shared_map;
    buffer_b = buffer_a + BUFFER_SIZE;
    ready_flag = (volatile uint8_t *)((char*)shared_map + 0x1000);

    printf("Mapped shared memory at %p\n", shared_map);
    printf("Buffer A: %p\n", (void*)buffer_a);
    printf("Buffer B: %p\n", (void*)buffer_b);
    printf("Ready flag: %p\n\n", (void*)ready_flag);

    printf("Monitoring (Ctrl+C to stop)...\n\n");

    int snapshot_count = 0;
    uint8_t last_flag = *ready_flag;
    struct timespec last_time, current_time;
    int buffer_count = 0;

    // Wait for first flag change
    printf("Waiting for PRU to set ready flag...\n");
    while (*ready_flag == 0 && keep_running) {
        usleep(10000);
    }
    last_flag = *ready_flag;
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    printf("PRU is running!\n\n");

    while (keep_running) {
        uint8_t flag = *ready_flag;

        if (flag != 0 && flag != last_flag) {
            // Flag changed - new buffer ready
            clock_gettime(CLOCK_MONOTONIC, &current_time);

            double elapsed = (current_time.tv_sec - last_time.tv_sec) +
                           (current_time.tv_nsec - last_time.tv_nsec) / 1e9;
            double sample_rate = BUFFER_SIZE / elapsed;

            buffer_count++;

            printf("=== Buffer %d ===\n", buffer_count);
            printf("Ready Flag: %d -> %d\n", last_flag, flag);
            printf("Time: %.6f sec (%.1f Hz sample rate)\n", elapsed, sample_rate);
            printf("\n");

            if (flag == 1) {
                analyze_buffer(buffer_a, "Buffer A");
            } else if (flag == 2) {
                analyze_buffer(buffer_b, "Buffer B");
            }
            printf("\n");

            last_flag = flag;
            last_time = current_time;
        }

        usleep(1000);  // Check every 1ms
    }

    printf("\nCleaning up...\n");
    munmap(shared_map, PRU_MEM_SIZE);
    close(mem_fd);

    return 0;
}
