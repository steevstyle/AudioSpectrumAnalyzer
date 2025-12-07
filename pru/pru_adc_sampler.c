#include <stdint.h>
#include "pru_cfg.h"
#include "resource_table_pru0.h"  // required for remoteproc

volatile register uint32_t __R30;
volatile register uint32_t __R31;

// ---------------------------------------------------------------------------
// Memory map
// ---------------------------------------------------------------------------
#define SHARED_RAM_BASE 0x00010000
#define BUFFER_A_BASE   ((volatile uint16_t *)(SHARED_RAM_BASE + 0x0000))
#define BUFFER_B_BASE   ((volatile uint16_t *)(SHARED_RAM_BASE + 0x0800))
#define FLAGS_BASE      ((volatile uint8_t *)(SHARED_RAM_BASE + 0x1000))

#define BUFFER_SIZE     1024

// ---------------------------------------------------------------------------
// ADC Registers (AM335x TRM)
// ---------------------------------------------------------------------------
#define ADC_BASE        0x44E0D000
#define ADC_CTRL        (*(volatile uint32_t *)(ADC_BASE + 0x40))
#define ADC_STEPENABLE  (*(volatile uint32_t *)(ADC_BASE + 0x54))
#define ADC_FIFO0DATA   (*(volatile uint32_t *)(ADC_BASE + 0x100))
#define ADC_FIFO0COUNT  (*(volatile uint32_t *)(ADC_BASE + 0xE4))
#define ADC_STEPCONFIG1 (*(volatile uint32_t *)(ADC_BASE + 0x64))
#define ADC_STEPDELAY1  (*(volatile uint32_t *)(ADC_BASE + 0x68))

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
#define PRU_FREQ_HZ      200000000
#define SAMPLE_RATE_HZ   48000
// Measured: loop overhead is ~10x, so divide target by 10
// Target: 4167 cycles total, minus ~400 for ADC/operations = 3767
// Actual loop count needed: 3767 / 10 ≈ 377
#define SAMPLE_DELAY_CYCLES 377

// ---------------------------------------------------------------------------
// Delay helper
// ---------------------------------------------------------------------------
static void delay_cycles(uint32_t cycles)
{
    volatile uint32_t i;
    for(i = 0; i < cycles; i++) {
        __asm__ __volatile__(" nop");
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main(void)
{
    volatile uint16_t *buffer_ptr = BUFFER_A_BASE;
    uint16_t sample_count = 0;
    uint8_t current_buffer = 1;  // 1=A, 2=B

    // Initialize ADC
    ADC_CTRL = 0x07;           // Enable ADC module
    delay_cycles(10000);       // small stabilization delay
    ADC_STEPCONFIG1 = 0x0;     // AIN0, one-shot
    ADC_STEPDELAY1  = 0x0;

    // Clear buffer ready flag
    FLAGS_BASE[0] = 0;

    while(1) {
        // Trigger step 1
        ADC_STEPENABLE = 0x2;

        // Wait for conversion (poll FIFO)
        while(ADC_FIFO0COUNT == 0) {}

        // Read sample
        uint16_t sample = (uint16_t)(ADC_FIFO0DATA & 0x0FFF);

        // Store in buffer
        buffer_ptr[sample_count++] = sample;

        // Check if buffer full
        if(sample_count >= BUFFER_SIZE) {
            // Set ready flag
            FLAGS_BASE[0] = current_buffer;

            // Swap buffer
            if(current_buffer == 1) {
                current_buffer = 2;
                buffer_ptr = BUFFER_B_BASE;
            } else {
                current_buffer = 1;
                buffer_ptr = BUFFER_A_BASE;
            }
            sample_count = 0;
        }

        // Timing delay
        delay_cycles(SAMPLE_DELAY_CYCLES);
    }
}

