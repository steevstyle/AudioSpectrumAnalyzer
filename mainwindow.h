#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h"
#include "dspthread.h"
#include "spectrumdata.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
            void updateSpectrum(const SpectrumData &data);

private:
    void setupPlot();

    QCustomPlot *m_plot;
    DSPThread *m_dspThread;
};

#endif