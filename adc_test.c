#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define ADC_TSC_BASE 0x44E0D000
#define MAP_SIZE 4096

// ADC Register offsets
#define CTRL         0x40
#define ADCSTAT      0x44
#define STEPCONFIG1  0x64
#define STEPDELAY1   0x68
#define FIFO0DATA    0x100
#define FIFO0COUNT   0xE4
#define STEPENABLE   0x54

int main() {
    int fd;
    void *map_base;
    volatile uint32_t *adc_mem;
    
    // Open /dev/mem
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("Cannot open /dev/mem - need to run as root");
        return 1;
    }
    
    // Map ADC registers
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ADC_TSC_BASE);
    if (map_base == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }
    
    adc_mem = (volatile uint32_t *)map_base;
    
    printf("BeagleBone Black ADC Test - Reading AIN0\n");
    printf("Press Ctrl+C to exit\n\n");
    printf("Expected voltage range: 0.0V to 1.8V\n\n");
    
    // Enable ADC module
    adc_mem[CTRL/4] = 0x07;  // Enable, no idle
    usleep(10000);
    
    // Configure Step 1 for AIN0
    adc_mem[STEPCONFIG1/4] = 0x00000000;  // AIN0, no averaging, one-shot
    adc_mem[STEPDELAY1/4] = 0x00000000;   // No delay
    
    // Enable Step 1
    adc_mem[STEPENABLE/4] = 0x02;  // Enable step 1
    
    while(1) {
        // Trigger conversion by enabling step
        adc_mem[STEPENABLE/4] = 0x02;
        usleep(10000);  // Wait for conversion
        
        // Check FIFO count
        uint32_t count = adc_mem[FIFO0COUNT/4];
        
        if (count > 0) {
            // Read from FIFO
            uint32_t data = adc_mem[FIFO0DATA/4];
            uint32_t value = data & 0xFFF;  // 12-bit value
            float voltage = (value / 4095.0) * 1.8;  // Convert to voltage
            
            printf("Raw: %4d (0x%03X)  Voltage: %.3fV  ", value, value, voltage);
            
            // Show a simple bar graph
            int bars = (int)(voltage / 1.8 * 40);
            printf("[");
            for(int i = 0; i < 40; i++) {
                if(i < bars) printf("=");
                else printf(" ");
            }
            printf("]\r");
            fflush(stdout);
        }
        
        usleep(100000);  // Update 10 times per second
    }
    
    // Cleanup
    munmap(map_base, MAP_SIZE);
    close(fd);
    
    return 0;
}
