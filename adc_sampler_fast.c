#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define ADC_TSC_BASE 0x44E0D000
#define MAP_SIZE 4096

// ADC Register offsets
#define CTRL         0x40
#define STEPCONFIG1  0x64
#define STEPDELAY1   0x68
#define STEPENABLE   0x54
#define FIFO0DATA    0x100
#define FIFO0COUNT   0xE4

// Shared memory for output
#define PRU_SHARED_MEM 0x4A310000
#define PRU_MEM_SIZE 0x3000
#define BUFFER_SIZE 1024

volatile int keep_running = 1;

void signal_handler(int signum) {
    keep_running = 0;
}

int main() {
    int mem_fd;
    void *adc_map, *shared_map;
    volatile uint32_t *adc_mem;
    volatile uint16_t *buffer_a, *buffer_b;
    volatile uint8_t *ready_flag;

    struct timespec buffer_start, buffer_end;
    uint16_t current_buffer = 1;  // 1=A, 2=B

    printf("ARM ADC Sampler (48 kHz) - FAST VERSION\n");
    printf("==========================================\n\n");

    signal(SIGINT, signal_handler);

    // Open /dev/mem
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Cannot open /dev/mem");
        return 1;
    }

    // Map ADC registers
    adc_map = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE,
                   MAP_SHARED, mem_fd, ADC_TSC_BASE);
    if (adc_map == MAP_FAILED) {
        perror("Cannot map ADC");
        close(mem_fd);
        return 1;
    }
    adc_mem = (volatile uint32_t *)adc_map;

    // Map shared memory
    shared_map = mmap(0, PRU_MEM_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED, mem_fd, PRU_SHARED_MEM);
    if (shared_map == MAP_FAILED) {
        perror("Cannot map shared memory");
        munmap(adc_map, MAP_SIZE);
        close(mem_fd);
        return 1;
    }

    buffer_a = (volatile uint16_t *)shared_map;
    buffer_b = buffer_a + BUFFER_SIZE;
    ready_flag = (volatile uint8_t *)((char*)shared_map + 0x1000);

    printf("Mapped memory successfully\n");

    // Initialize ADC
    printf("Initializing ADC...\n");
    adc_mem[CTRL/4] = 0x07;  // Enable ADC
    usleep(10000);

    // Configure Step 1 for AIN0 (one-shot mode, no averaging)
    adc_mem[STEPCONFIG1/4] = 0x00000000;  // AIN0, no averaging, one-shot
    adc_mem[STEPDELAY1/4] = 0x00000000;   // No delay

    *ready_flag = 0;  // Clear flag

    printf("Sampling at 48 kHz (no waiting for reader)...\n");
    printf("Press Ctrl+C to stop\n\n");

    int sample_count = 0;
    int buffer_count = 0;
    volatile uint16_t *current_buf = buffer_a;

    uint16_t min_sample = 4095, max_sample = 0;

    clock_gettime(CLOCK_MONOTONIC, &buffer_start);

    while (keep_running) {
        // Trigger ADC conversion
        adc_mem[STEPENABLE/4] = 0x02;

        // Busy-wait for conversion (no usleep!)
        int timeout = 10000;
        while (timeout-- > 0 && (adc_mem[FIFO0COUNT/4] == 0)) {
            // Busy wait
        }

        // Read sample
        uint32_t data = adc_mem[FIFO0DATA/4];
        uint16_t sample = data & 0x0FFF;

        // Track stats
        if (sample < min_sample) min_sample = sample;
        if (sample > max_sample) max_sample = sample;

        // Store in current buffer
        current_buf[sample_count++] = sample;

        // Check if buffer full
        if (sample_count >= BUFFER_SIZE) {
            clock_gettime(CLOCK_MONOTONIC, &buffer_end);

            // Calculate actual sample rate
            long buffer_time_us = (buffer_end.tv_sec - buffer_start.tv_sec) * 1000000L +
                                  (buffer_end.tv_nsec - buffer_start.tv_nsec) / 1000L;
            double actual_rate = 1000000.0 * BUFFER_SIZE / buffer_time_us;

            printf("Buffer %d: %.1f Hz | Min: %d (%.3fV) Max: %d (%.3fV) | Flag was: %d\n",
                   buffer_count, actual_rate,
                   min_sample, min_sample * 1.8 / 4095.0,
                   max_sample, max_sample * 1.8 / 4095.0,
                   *ready_flag);

            // Set ready flag (don't wait for clear)
            *ready_flag = current_buffer;

            buffer_count++;
            clock_gettime(CLOCK_MONOTONIC, &buffer_start);

            // Switch buffers
            if (current_buffer == 1) {
                current_buffer = 2;
                current_buf = buffer_b;
            } else {
                current_buffer = 1;
                current_buf = buffer_a;
            }

            sample_count = 0;
            min_sample = 4095;
            max_sample = 0;
        }

        // Timing loop - calibrate this value to hit 48 kHz
        // Adjust based on actual measured rate shown in output
        for (volatile int d = 0; d < 550; d++) {
            // Busy wait
        }
    }

    printf("\n\nStopping...\n");

    // Cleanup
    munmap(adc_map, MAP_SIZE);
    munmap(shared_map, PRU_MEM_SIZE);
    close(mem_fd);

    printf("Done. Sampled %d buffers.\n", buffer_count);
    return 0;
}
