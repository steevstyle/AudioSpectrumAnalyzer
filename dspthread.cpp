#include "dspthread.h"
#include <fftw3.h>
#include <cmath>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define PRU_SHARED_MEM 0x4A310000  // PRU shared memory address
#define PRU_MEM_SIZE 0x3000        // 12KB

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
    return nullptr;
    /* Commenting out for now
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
    */
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
    for (int i = 0; i < numSamples; i++) {
	double t = i / (double)SAMPLE_RATE;
	double value = 0.9 + 0.3 * sin(2.0 * M_PI * 10000.0 * t);
	samples.append(value);
    }
    return samples;
    /* DISABLE PRU FOR NOW
    if (!m_pruBuffer) {
        // Generate test data (sine wave at 1 kHz)
        for (int i = 0; i < numSamples; i++) {
            double t = i / (double)SAMPLE_RATE;
            double value = 0.9 + 0.3 * sin(2.0 * M_PI * 1000.0 * t);
            samples.append(value);
        }
        return samples;
    //}
    
    // Read from PRU buffer
    for (int i = 0; i < numSamples; i++) {
        uint16_t raw = m_pruBuffer[i];
        double voltage = (raw / 4095.0) * 1.8;  // Convert to voltage
        samples.append(voltage);
    }

    return samples;
    */
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

    // DC component
    //double mag = fabs(m_fftOutput[0]);
    //double db = 20.0 * log10(mag + 1e-10);
    //magnitudes.append(qMax(db, -80.0));

    // Other bins (half-complex format)
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        double real = m_fftOutput[i];
        double imag = m_fftOutput[FFT_SIZE - i];
        mag = sqrt(real * real + imag * imag);
        db = 20.0 * log10(mag + 1e-10);
        magnitudes.append(qMax(db, -80.0));
    }

    // Nyquist component
    mag = fabs(m_fftOutput[FFT_SIZE / 2]);
    db = 20.0 * log10(mag + 1e-10);
    magnitudes.append(qMax(db, -80.0));
}

void DSPThread::run() {
    m_running = true;

    // Try to map PRU memory
    // mapPRUMemory();

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

        // ~30 Hz update rate
        usleep(33333);
    }
}

void DSPThread::stop() {
    m_running = false;
    wait();
}
