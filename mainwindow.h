#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h"
#include "dspthread.h"
#include "spectrumdata.h"
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QMutex>
#include <QMutexLocker>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
            // void updateSpectrum(const SpectrumData &data);
	    void cacheSpectrum(const SpectrumData &data);
            void onResetDisplayClicked();
            void onSmoothingChanged(int value);
            void onToggleDisplayMode();

private:
    void setupPlot();
    void setupSpectrogram();
    void refreshPlot();
    void refreshSpectrogram();

    QCustomPlot *m_plot;
    QCPColorMap *m_colorMap;
    DSPThread *m_dspThread;
    QPushButton *m_resetButton;
    QPushButton *m_toggleButton;
    QSlider *m_smoothingSlider;
    QLabel *m_smoothingLabel;

    QMutex m_spectrumMutex;
    SpectrumData m_cachedSpectrum;
    bool m_hasCachedSpectrum;
    double m_smoothingAlpha;

    bool m_spectrogramMode;
    int m_spectrogramRows;
    static const int MAX_SPECTROGRAM_ROWS = 200;

    QTimer *m_uiTimer;
};

#endif