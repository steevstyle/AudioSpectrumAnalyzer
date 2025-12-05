#include "mainwindow.h"
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
{
    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Create plot
    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);
    setCentralWidget(centralWidget);

    setupPlot();

    // Create and start DSP thread
    m_dspThread = new DSPThread(this);
    connect(m_dspThread, &DSPThread::spectrumReady,
            this, &MainWindow::updateSpectrum);
    m_dspThread->start();
}

MainWindow::~MainWindow() {
    m_dspThread->stop();
}

void MainWindow::setupPlot() {
    // Configure X axis (Frequency)
    m_plot->xAxis->setLabel("Frequency (Hz)");
    m_plot->xAxis->setRange(0, 24000);

    // Configure Y axis (Magnitude)
    m_plot->yAxis->setLabel("Magnitude (dB)");
    m_plot->yAxis->setRange(-80, 0);

    // Dark background
    m_plot->setBackground(QColor(20, 20, 20));

    // White axes
    m_plot->xAxis->setBasePen(QPen(Qt::white));
    m_plot->yAxis->setBasePen(QPen(Qt::white));
    m_plot->xAxis->setTickPen(QPen(Qt::white));
    m_plot->yAxis->setTickPen(QPen(Qt::white));
    m_plot->xAxis->setTickLabelColor(Qt::white);
    m_plot->yAxis->setTickLabelColor(Qt::white);
    m_plot->xAxis->setLabelColor(Qt::white);
    m_plot->yAxis->setLabelColor(Qt::white);

    // Grid
    m_plot->xAxis->grid()->setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));

    // Add graph
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(QColor(0, 255, 0), 2));  // Green, 2px
}

void MainWindow::updateSpectrum(const SpectrumData &data) {
    // Update plot

    QVector<double> freqs = data.frequencies;
    QVector<double> mags  = data.magnitudes;

    // Zero out DC component
    if (!mags.isEmpty()) {
        mags[0] = -80.0;
    }

    m_plot->graph(0)->setData(freqs, mags);
    m_plot->replot();
}