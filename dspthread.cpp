#include "dspthread.h"
#include <fftw3.h>
#include <cmath>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <QDebug>

#define PRU_SHARED_MEM 0x4A310000  // PRU shared memory address, from AM335x TRM
#define PRU_MEM_SIZE 0x3000        // 12KB, total shared RAM avail

DSPThread::DSPThread(QObject *parent)
        : QThread(parent)
        , m_running(false)
        , m_pruBuffer(nullptr)
        , m_pruMemFd(-1)
        , m_fftPlan(nullptr)
        , m_fftInput(nullptr)
        , m_fftOutput(nullptr)
{
    initFFT();
}

DSPThread::~DSPThread() {
    stop();
    cleanupFFT();
    unmapPRUMemory();
}

void DSPThread::initFFT() {
    m_fftInput = (double*)fftw_malloc(sizeof(double) * FFT_SIZE);
    m_fftOutput = (double*)fftw_malloc(sizeof(double) * FFT_SIZE);

    // Create FFTW plan (real to half-complex)
    // see https://www.fftw.org/fftw3_doc/Using-Plans.html
    // see https://www.fftw.org/fftw3_doc/The-Halfcomplex_002dformat-DFT.html
    m_fftPlan = fftw_plan_r2r_1d(FFT_SIZE, m_fftInput, m_fftOutput,
                                 FFTW_R2HC, FFTW_ESTIMATE);
}

void DSPThread::cleanupFFT() {
    if (m_fftPlan) {
        fftw_destroy_plan((fftw_plan)m_fftPlan);
        m_fftPlan = nullptr;
    }
    if (m_fftInput) {
        fftw_free(m_fftInput);
        m_fftInput = nullptr;
    }
    if (m_fftOutput) {
        fftw_free(m_fftOutput);
        m_fftOutput = nullptr;
    }
}

void* DSPThread::mapPRUMemory() {
    m_pruMemFd = open("/dev/mem", O_RDWR | O_SYNC);
    if (m_pruMemFd < 0) {
        return nullptr;
    }

    void* mapped = mmap(0, PRU_MEM_SIZE, PROT_READ | PROT_WRITE,
                        MAP_SHARED, m_pruMemFd, PRU_SHARED_MEM);

    if (mapped == MAP_FAILED) {
        close(m_pruMemFd);
        m_pruMemFd = -1;
        return nullptr;
    }

    m_pruBuffer = (uint16_t*)mapped;
    return mapped;
}

void DSPThread::unmapPRUMemory() {
    if (m_pruBuffer) {
        munmap(m_pruBuffer, PRU_MEM_SIZE);
        m_pruBuffer = nullptr;
    }
    if (m_pruMemFd >= 0) {
        close(m_pruMemFd);
        m_pruMemFd = -1;
    }
}

QVector<double> DSPThread::readPRUSamples(int numSamples) {
    QVector<double> samples;

    if (!m_pruBuffer) {
        //fallback synthetic signal (10kHz sine wave)
        for (int i = 0; i < numSamples; i++) {
            double t = i / (double)SAMPLE_RATE;
            double value = 0.9 + 0.3 * sin(2.0 * M_PI * 10000.0 * t);
            samples.append(value);
        }
        return samples;
    }

    // Get pointer to ready flag (at offset 0x1000)
    volatile uint8_t* ready_flag = (volatile uint8_t*)((char*)m_pruBuffer + 0x1000);

    // Track which buffer we read last to detect new data
    static uint8_t last_buffer_read = 0;

    // Wait for flag to change (indicating new buffer ready)
    int wait_count = 0;
    while (*ready_flag == last_buffer_read && wait_count < 1000) {
        usleep(100);  // Wait 100us between checks
        wait_count++;
    }

    // Occasional warning if stuck
    // We should only see this warning if we forget to start PRU firmware
    static int stuck_count = 0;
    if (*ready_flag == last_buffer_read) {
        if (++stuck_count >= 10) {
            qDebug() << "WARNING: Buffer flag stuck at" << *ready_flag;
            stuck_count = 0;
        }
    } else {
        stuck_count = 0;
    }

    uint8_t buffer_ready = *ready_flag;  // 1=A, 2=B
    last_buffer_read = buffer_ready;  // Remember which buffer we just read

    volatile uint16_t* read_buffer = m_pruBuffer;  // Default to A

    if (buffer_ready == 2) {
        // Buffer B is ready (at offset BUFFER_SIZE)
        read_buffer = m_pruBuffer + 1024;
    }

    // Read from the ready buffer and calculate mean for DC removal
    // ADC yields 12-bit resolution and a range from 0.0-1.8V
    double sum = 0.0;
    for (int i = 0; i < numSamples; i++) {
        uint16_t raw = read_buffer[i];
        double voltage = (raw / 4095.0) * 1.8;  // Convert to voltage
        samples.append(voltage);
        sum += voltage;
    }

    // Remove DC offset (critical for clean FFT)
    double dc_offset = sum / numSamples;
    for (int i = 0; i < numSamples; i++) {
        samples[i] -= dc_offset;
    }

    // Calculate basic statistics for debugging (print occasionally to reduce overhead)
    static int debug_counter = 0;
    if (++debug_counter >= 50) {  // Print every 50 buffers (~1 second at 48 kHz)
        uint16_t min_raw = 4095, max_raw = 0;
        sum = 0.0;
        for (int i = 0; i < numSamples; i++) {
            uint16_t raw = read_buffer[i];
            if (raw < min_raw) min_raw = raw;
            if (raw > max_raw) max_raw = raw;
            sum += raw;
        }
        double avg_raw = sum / numSamples;

        qDebug() << "Buffer stats - Min:" << min_raw << "(" << (min_raw * 1.8 / 4095.0) << "V)"
                 << "Max:" << max_raw << "(" << (max_raw * 1.8 / 4095.0) << "V)"
                 << "Avg:" << avg_raw << "(" << (avg_raw * 1.8 / 4095.0) << "V)";
        debug_counter = 0;
    }

    return samples;
}

void DSPThread::applyHannWindow(QVector<double> &samples) {
    int n = samples.size();
    for (int i = 0; i < n; i++) {
        double window = 0.5 * (1.0 - cos(2.0 * M_PI * i / (n - 1)));
        samples[i] *= window;
    }
}

void DSPThread::computeFFT(const QVector<double> &samples,
                           QVector<double> &magnitudes) {
    // Copy samples to FFT input buffer
    for (int i = 0; i < FFT_SIZE; i++) {
        m_fftInput[i] = samples[i];
    }

    // Execute FFT
    fftw_execute((fftw_plan)m_fftPlan);

    // Compute magnitudes (convert to dB)
    magnitudes.clear();

    // Normalization factors:
    // - FFT_SIZE: FFTW r2r doesn't normalize
    // - 2.0: Convert single-sided spectrum to power
    // - Hann window coherent gain: 0.5
    // Reference: Full-scale sine wave (0.9V p-p = 0.45V amplitude)
    const double FULL_SCALE_VOLTAGE = 0.9;  // Adjust based on your signal levels
    const double WINDOW_GAIN = 0.5;  // Hann window coherent gain
    const double NORMALIZATION = FFT_SIZE * WINDOW_GAIN;

    // DC component (skip, not useful for visualization)

    // Other bins (half-complex format)
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        double real = m_fftOutput[i];
        double imag = m_fftOutput[FFT_SIZE - i];
        double mag = sqrt(real * real + imag * imag);

        // Normalize and convert to voltage amplitude
        double voltage_amplitude = (mag / NORMALIZATION) * 2.0;

        // Convert to dB relative to full scale
        double db = 20.0 * log10(voltage_amplitude / FULL_SCALE_VOLTAGE + 1e-10);
        magnitudes.append(qMax(db, -80.0));
    }

    // Nyquist component (24kHz)
    double mag_nyq = fabs(m_fftOutput[FFT_SIZE / 2]);
    double voltage_amplitude_nyq = (mag_nyq / NORMALIZATION);
    double db_nyq = 20.0 * log10(voltage_amplitude_nyq / FULL_SCALE_VOLTAGE + 1e-10);
    magnitudes.append(qMax(db_nyq, -80.0));
}

void DSPThread::run() {
    m_running = true;

    // Try to map PRU memory
    mapPRUMemory();
    if (m_pruBuffer) {
        qDebug() << "Successfully mapped shared memory - using real ADC data";
    } else {
        qDebug() << "Could not map shared memory - using test signal";
    }

    while (m_running) {
        // Read samples
        QVector<double> samples = readPRUSamples(FFT_SIZE);

        // Apply window
        applyHannWindow(samples);

        // Compute FFT
        SpectrumData data;
        data.sampleRate = SAMPLE_RATE;
        data.fftSize = FFT_SIZE;
        data.numBins = FFT_SIZE / 2 + 1;

        computeFFT(samples, data.magnitudes);

        // Generate frequency bins
        for (int i = 1; i < data.numBins; i++) {
            double freq = i * SAMPLE_RATE / (double)FFT_SIZE;
            data.frequencies.append(freq);
        }

        // Emit data
        emit spectrumReady(data);

        // No fixed delay - pace based on PRU buffer rate
        // At 48 kHz, buffers arrive every ~21ms naturally
    }
}

void DSPThread::stop() {
    m_running = false;
    wait();
}
