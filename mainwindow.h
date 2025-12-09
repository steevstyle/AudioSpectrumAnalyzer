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

private:
    void setupPlot();
    void refreshPlot();

    QCustomPlot *m_plot;
    DSPThread *m_dspThread;
    QPushButton *m_resetButton;
    QSlider *m_smoothingSlider;
    QLabel *m_smoothingLabel;

    QMutex m_spectrumMutex;
    SpectrumData m_cachedSpectrum;
    bool m_hasCachedSpectrum;
    double m_smoothingAlpha;

    QTimer *m_uiTimer;
};

#endif