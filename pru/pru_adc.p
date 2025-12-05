// ============================================================================
// PRU ADC Sampler with Ping-Pong Buffers
// Samples AIN0 at 48 kHz and stores in alternating buffers
// ============================================================================

.origin 0
.entrypoint START

// ============================================================================
// Register Assignments
// ============================================================================
#define REG_ADC_BASE        r0      // ADC register base address
#define REG_SAMPLE_COUNT    r1      // Current sample count (0-1023)
#define REG_BUFFER_PTR      r2      // Current write pointer
#define REG_TEMP            r3      // Temporary register for operations
#define REG_DELAY           r4      // Delay loop counter
#define REG_ADC_VALUE       r5      // ADC sample value
#define REG_CURRENT_BUFFER  r6      // Which buffer (1=A, 2=B)
#define REG_FLAGS_ADDR      r7      // Address of control flags

// ============================================================================
// Memory Map
// ============================================================================
#define SHARED_RAM_BASE     0x00010000
#define BUFFER_A_BASE       0x00010000  // First 2KB
#define BUFFER_B_BASE       0x00010800  // Second 2KB
#define FLAGS_BASE          0x00011000  // Control flags
#define BUFFER_SIZE         1024        // Samples per buffer

// ============================================================================
// ADC Register Offsets (from TI AM335x TRM)
// ============================================================================
#define ADC_BASE_ADDR       0x44E0D000
#define ADC_CTRL            0x40        // Control register
#define ADC_STEPCONFIG1     0x64        // Step 1 config
#define ADC_STEPDELAY1      0x68        // Step 1 delay
#define ADC_STEPENABLE      0x54        // Step enable
#define ADC_FIFO0DATA       0x100       // FIFO data register
#define ADC_FIFO0COUNT      0xE4        // FIFO count register

// ============================================================================
// Timing Constants
// ============================================================================
// PRU runs at 200 MHz, sample at 48 kHz
// Cycles per sample = 200,000,000 / 48,000 = 4,167 cycles
#define SAMPLE_DELAY        3800        // Delay cycles (adjust for overhead)

// ============================================================================
// Main Program
// ============================================================================
START:
    // Initialize ADC base address
    MOV REG_ADC_BASE, ADC_BASE_ADDR

    // Enable ADC module (write 0x07 to CTRL register)
    MOV REG_TEMP, 0x07
    SBBO REG_TEMP, REG_ADC_BASE, ADC_CTRL, 4

    // Small delay for ADC to stabilize
    MOV REG_DELAY, 10000
INIT_DELAY:
    SUB REG_DELAY, REG_DELAY, 1
    QBNE INIT_DELAY, REG_DELAY, 0

    // Configure Step 1 for AIN0 (channel 0, one-shot mode)
    MOV REG_TEMP, 0x00000000
    SBBO REG_TEMP, REG_ADC_BASE, ADC_STEPCONFIG1, 4
    SBBO REG_TEMP, REG_ADC_BASE, ADC_STEPDELAY1, 4

    // Initialize buffer management
    MOV REG_BUFFER_PTR, BUFFER_A_BASE      // Start with Buffer A
    MOV REG_SAMPLE_COUNT, 0                 // No samples yet
    MOV REG_CURRENT_BUFFER, 1               // 1 = Buffer A, 2 = Buffer B
    MOV REG_FLAGS_ADDR, FLAGS_BASE          // Flags address

    // Clear ready flag (0 = no buffer ready)
    MOV REG_TEMP, 0
    SBBO REG_TEMP, REG_FLAGS_ADDR, 0, 1

// ============================================================================
// Main Sampling Loop
// ============================================================================
SAMPLE_LOOP:
    // -------------------------
    // 1. Trigger ADC Conversion
    // -------------------------
    MOV REG_TEMP, 0x02                      // Enable step 1
    SBBO REG_TEMP, REG_ADC_BASE, ADC_STEPENABLE, 4

    // -------------------------
    // 2. Wait for conversion with delay + poll
    // -------------------------
    // Most of conversion time (14 us = ~2800 cycles)
    MOV REG_DELAY, 2800
CONVERSION_DELAY:
    SUB REG_DELAY, REG_DELAY, 1
    QBNE CONVERSION_DELAY, REG_DELAY, 0

    // Poll FIFO count until data ready
POLL_FIFO:
    LBBO REG_TEMP, REG_ADC_BASE, ADC_FIFO0COUNT, 4
    QBEQ POLL_FIFO, REG_TEMP, 0            // Loop while count = 0

    // -------------------------
    // 3. Read ADC Result
    // -------------------------
    LBBO REG_ADC_VALUE, REG_ADC_BASE, ADC_FIFO0DATA, 4
    AND REG_ADC_VALUE, REG_ADC_VALUE, 0x0FFF   // Mask to 12 bits

    // -------------------------
    // 4. Store Sample in Current Buffer
    // -------------------------
    SBBO REG_ADC_VALUE, REG_BUFFER_PTR, 0, 2   // Write 2 bytes (16-bit)

    // -------------------------
    // 5. Update Pointers and Counters
    // -------------------------
    ADD REG_BUFFER_PTR, REG_BUFFER_PTR, 2      // Move to next sample (2 bytes)
    ADD REG_SAMPLE_COUNT, REG_SAMPLE_COUNT, 1  // Increment counter

    // -------------------------
    // 6. Check if Buffer is Full
    // -------------------------
    QBNE TIMING_DELAY, REG_SAMPLE_COUNT, BUFFER_SIZE

    // Buffer is full! Switch buffers
    SBBO REG_CURRENT_BUFFER, REG_FLAGS_ADDR, 0, 1  // Set ready flag

    // Swap to other buffer
    QBEQ SWITCH_TO_B, REG_CURRENT_BUFFER, 1

SWITCH_TO_A:
    MOV REG_CURRENT_BUFFER, 1
    MOV REG_BUFFER_PTR, BUFFER_A_BASE
    JMP BUFFER_SWITCHED

SWITCH_TO_B:
    MOV REG_CURRENT_BUFFER, 2
    MOV REG_BUFFER_PTR, BUFFER_B_BASE

BUFFER_SWITCHED:
    MOV REG_SAMPLE_COUNT, 0                    // Reset counter

    // -------------------------
    // 7. Timing Delay (maintain 48 kHz rate)
    // -------------------------
TIMING_DELAY:
    MOV REG_DELAY, SAMPLE_DELAY
DELAY_LOOP:
    SUB REG_DELAY, REG_DELAY, 1
    QBNE DELAY_LOOP, REG_DELAY, 0

    // -------------------------
    // 8. Loop Forever
    // -------------------------
    JMP SAMPLE_LOOP

// Should never reach here
HALT