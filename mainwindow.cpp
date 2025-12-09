#include "mainwindow.h"
#include <QVBoxLayout>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
{
    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Create plot
    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);

    // Create Reset button
    m_resetButton = new QPushButton("Reset Display", this);
    layout->addWidget(m_resetButton);
    connect(m_resetButton, &QPushButton::clicked,
            this, &MainWindow::onResetDisplayClicked);

    setCentralWidget(centralWidget);

    setupPlot();

    // Create and start DSP thread
    m_dspThread = new DSPThread(this);
    connect(m_dspThread, &DSPThread::spectrumReady,
            this, &MainWindow::cacheSpectrum, Qt::QueuedConnection);
    // UI refresh timer (~30Hz)
    m_uiTimer = new QTimer(this);
    connect(m_uiTimer, &QTimer::timeout, this, &MainWindow::refreshPlot);
    m_uiTimer->start(33);
    m_dspThread->start();
}

MainWindow::~MainWindow() {
    if (m_dspThread) {
        m_dspThread->stop();
    }
}

void MainWindow::cacheSpectrum(const SpectrumData &data) {
    QMutexLocker locker(&m_spectrumMutex);
    if (!m_hasCachedSpectrum) {    
        m_cachedSpectrum = data;
        m_hasCachedSpectrum = true;
    } else {
        // Exponential averaging of current and previous spectra
        const double alpha = 0.85;

        for (int i = 0; i < data.magnitudes.size(); i++) {
            m_cachedSpectrum.magnitudes[i] = alpha * m_cachedSpectrum.magnitudes[i] + (1.0 - alpha) * data.magnitudes[i];
        }
}

void MainWindow::refreshPlot() {
    if (!m_hasCachedSpectrum)
        return;

    SpectrumData localCopy;

    { QMutexLocker locker(&m_spectrumMutex);
      localCopy = m_cachedSpectrum;
    }

    m_plot->graph(0)->setData(localCopy.frequencies,
                              localCopy.magnitudes);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::setupPlot() {
    // Configure X axis (Frequency)
    m_plot->xAxis->setLabel("Frequency (Hz)");

    // Use logarithmic scale for audio frequencies
    m_plot->xAxis->setScaleType(QCPAxis::stLogarithmic);

    // Ignore DC (0 Hz): start just above bin 0, around 20 Hz, up to 20 kHz
    m_plot->xAxis->setRange(31.5, 20000);

    // Use a text ticker to place ticks at standard audio band center frequencies
    QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
    textTicker->addTick(63,    "63");
    textTicker->addTick(125,   "125");
    textTicker->addTick(250,   "250");
    textTicker->addTick(500,   "500");
    textTicker->addTick(1000,  "1k");
    textTicker->addTick(2000,  "2k");
    textTicker->addTick(4000,  "4k");
    textTicker->addTick(8000,  "8k");
    textTicker->addTick(16000, "16k");
    m_plot->xAxis->setTicker(textTicker);

    // Keep a reasonable tick length
    m_plot->xAxis->setTickLength(5, 2);

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

/*
void MainWindow::updateSpectrum(const SpectrumData &data) {
    // Drop updates if we're still processing the last one
    static bool processing = false;
    if (processing) {
        return;  // Skip this update, GUI is busy
    }
    processing = true;

    // Update plot
    QVector<double> freqs = data.frequencies;
    QVector<double> mags  = data.magnitudes;

    // Zero out DC component
    //if (!mags.isEmpty()) {
    //    mags[0] = -80.0;
    //}

    m_plot->graph(0)->setData(freqs, mags);
    m_plot->replot();

    processing = false;
}
*/

void MainWindow::onResetDisplayClicked() {
    // Clear plot data
    if (m_plot && m_plot->graphCount() > 0) {
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            m_plot->graph(i)->data()->clear();
        }
        m_plot->replot();
    }

    // Quit the application 
    QCoreApplication::quit();
}
