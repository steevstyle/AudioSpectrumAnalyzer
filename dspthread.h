#ifndef DSPTHREAD_H
#define DSPTHREAD_H

#include <QThread>
#include <atomic>
#include "spectrumdata.h"

class DSPThread : public QThread {
    Q_OBJECT

public:
    DSPThread(QObject *parent = nullptr);
    ~DSPThread();

    void stop();

    signals:
            void spectrumReady(const SpectrumData &data);

protected:
    void run() override;

private:
    // PRU interface
    void* mapPRUMemory();
    void unmapPRUMemory();
    QVector<double> readPRUSamples(int numSamples);

    uint16_t* m_pruBuffer;
    int m_pruMemFd;

    // FFT processing
    void initFFT();
    void cleanupFFT();
    void computeFFT(const QVector<double> &samples,
                    QVector<double> &magnitudes);
    void applyHannWindow(QVector<double> &samples);

    // State
    std::atomic<bool> m_running;

    // FFT objects (FFTW)
    void* m_fftPlan;
    double* m_fftInput;
    double* m_fftOutput;

    // Constants
    static const int FFT_SIZE = 1024;
    static const int SAMPLE_RATE = 48000;
};

#endif