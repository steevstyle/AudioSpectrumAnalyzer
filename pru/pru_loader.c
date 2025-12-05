#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#define PRU_SHARED_MEM 0x4A310000
#define PRU_MEM_SIZE 0x3000

int main() {
    int fd;
    void *shared_mem;
    volatile uint16_t *buffer_a;
    volatile uint8_t *flags;

    printf("PRU ADC Test Program\n");
    printf("====================\n\n");

    // Map PRU shared memory
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Cannot open /dev/mem");
        return 1;
    }

    shared_mem = mmap(0, PRU_MEM_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, PRU_SHARED_MEM);

    if (shared_mem == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }

    buffer_a = (volatile uint16_t *)shared_mem;              // 0x00010000
    flags = (volatile uint8_t *)((char*)shared_mem + 0x1000); // 0x00011000

    printf("Mapped PRU shared memory\n");
    printf("Buffer A: %p\n", buffer_a);
    printf("Flags: %p\n\n", flags);

    // Clear the ready flag
    *flags = 0;

    printf("Instructions:\n");
    printf("1. Load PRU firmware manually:\n");
    printf("   echo 'stop' > /sys/class/remoteproc/remoteproc0/state\n");
    printf("   cp pru_adc.bin /lib/firmware/am335x-pru0-fw\n");
    printf("   echo 'start' > /sys/class/remoteproc/remoteproc0/state\n\n");
    printf("2. Press Enter when PRU is running...\n");
    getchar();

    printf("\nMonitoring buffers (Ctrl+C to stop)...\n\n");

    int buffer_count = 0;
    uint8_t last_flag = 0;

    while (1) {
        uint8_t flag = *flags;

        if (flag != 0 && flag != last_flag) {
            buffer_count++;

            if (flag == 1) {
                // Buffer A ready
                printf("Buffer %d: Buffer A ready\n", buffer_count);

                // Show first 10 samples
                printf("  First 10 samples: ");
                for (int i = 0; i < 10; i++) {
                    printf("%4d ", buffer_a[i]);
                }
                printf("\n");

                // Show min/max
                uint16_t min = 4095, max = 0;
                for (int i = 0; i < 1024; i++) {
                    if (buffer_a[i] < min) min = buffer_a[i];
                    if (buffer_a[i] > max) max = buffer_a[i];
                }
                printf("  Min: %d (%.3fV)  Max: %d (%.3fV)\n",
                       min, min * 1.8 / 4095.0,
                       max, max * 1.8 / 4095.0);
            }
            else if (flag == 2) {
                // Buffer B ready
                volatile uint16_t *buffer_b = buffer_a + 1024;
                printf("Buffer %d: Buffer B ready\n", buffer_count);

                printf("  First 10 samples: ");
                for (int i = 0; i < 10; i++) {
                    printf("%4d ", buffer_b[i]);
                }
                printf("\n");

                uint16_t min = 4095, max = 0;
                for (int i = 0; i < 1024; i++) {
                    if (buffer_b[i] < min) min = buffer_b[i];
                    if (buffer_b[i] > max) max = buffer_b[i];
                }
                printf("  Min: %d (%.3fV)  Max: %d (%.3fV)\n",
                       min, min * 1.8 / 4095.0,
                       max, max * 1.8 / 4095.0);
            }

            // Clear flag (acknowledge)
            *flags = 0;
            last_flag = flag;

            printf("\n");
        }

        usleep(10000);  // Check every 10ms
    }

    munmap(shared_mem, PRU_MEM_SIZE);
    close(fd);

    return 0;
}