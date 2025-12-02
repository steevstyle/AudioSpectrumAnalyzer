#ifndef SPECTRUMDATA_H
#define SPECTRUMDATA_H

#include <QVector>
#include <cstdint>

// Simple spectrum data structure
struct SpectrumData {
    QVector<double> frequencies;  // Hz (0 to 24000)
    QVector<double> magnitudes;   // dB (-80 to 0)
    uint32_t sampleRate;          // 48000
    uint32_t fftSize;             // 1024
    uint32_t numBins;             // 512
};

#endif