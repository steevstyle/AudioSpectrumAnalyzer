#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h"
#include "dspthread.h"
#include "spectrumdata.h"
#include <QPushButton>
#include <QMutex>
#include <QMutexLocker>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
            // void updateSpectrum(const SpectrumData &data);
	    void cacheSpectrum(const SpectrumData &data);
            void onResetDisplayClicked();

private:
    void setupPlot();
    void refreshPlot();

    QCustomPlot *m_plot;
    DSPThread *m_dspThread;
    QPushButton *m_resetButton;

    QMutex m_spectrumMutex;
    SpectrumData m_cachedSpectrum;
    bool m_hasCachedSpectrum;

    QTimer *m_uiTimer;
};

#endif