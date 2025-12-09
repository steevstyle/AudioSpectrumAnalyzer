#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
        , m_hasCachedSpectrum(false)
        , m_smoothingAlpha(0.85)
        , m_spectrogramMode(false)
        , m_spectrogramRows(0)
{
    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Create plot
    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);

    // Create smoothing slider at bottom
    QHBoxLayout *sliderLayout = new QHBoxLayout();
    m_smoothingLabel = new QLabel("Smoothing: 0.85", this);
    m_smoothingLabel->setStyleSheet("color: white;");
    sliderLayout->addWidget(m_smoothingLabel);

    m_smoothingSlider = new QSlider(Qt::Horizontal, this);
    m_smoothingSlider->setRange(0, 95);  // 0.00 to 0.95
    m_smoothingSlider->setValue(85);     // 0.85 default
    m_smoothingSlider->setStyleSheet("QSlider { background: transparent; }");
    sliderLayout->addWidget(m_smoothingSlider);
    connect(m_smoothingSlider, &QSlider::valueChanged,
            this, &MainWindow::onSmoothingChanged);

    layout->addLayout(sliderLayout);

    setCentralWidget(centralWidget);

    // Create Reset button - overlay in top right of plot
    m_resetButton = new QPushButton("X", m_plot);
    m_resetButton->setFixedSize(30, 30);
    // Position assuming typical window width (will adjust with resize event if needed)
    m_resetButton->move(this->width() - 40, 10);
    m_resetButton->setStyleSheet(
        "QPushButton { "
        "  background-color: rgba(200, 0, 0, 180); "
        "  color: white; "
        "  border: 1px solid white; "
        "  border-radius: 15px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "  background-color: rgba(255, 0, 0, 220); "
        "}"
    );
    m_resetButton->raise();  // Ensure it's on top
    connect(m_resetButton, &QPushButton::clicked,
            this, &MainWindow::onResetDisplayClicked);

    // Create Toggle button - overlay next to reset button
    m_toggleButton = new QPushButton("Spec", m_plot);
    m_toggleButton->setFixedSize(50, 30);
    m_toggleButton->setStyleSheet(
        "QPushButton { "
        "  background-color: rgba(0, 120, 200, 180); "
        "  color: white; "
        "  border: 1px solid white; "
        "  border-radius: 5px; "
        "  font-weight: bold; "
        "  font-size: 10px; "
        "} "
        "QPushButton:hover { "
        "  background-color: rgba(0, 150, 255, 220); "
        "}"
    );
    m_toggleButton->raise();
    connect(m_toggleButton, &QPushButton::clicked,
            this, &MainWindow::onToggleDisplayMode);

    setupPlot();
    setupSpectrogram();

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
        // Use the dynamic smoothing factor from the slider
        double alpha = m_smoothingAlpha;

        // Ensure sizes match before accessing elements
        if (m_cachedSpectrum.magnitudes.size() != data.magnitudes.size()) {
            // Size mismatch - just replace with new data
            m_cachedSpectrum = data;
        } else {
            // Apply exponential averaging
            for (int i = 0; i < data.magnitudes.size(); i++) {
                m_cachedSpectrum.magnitudes[i] = alpha * m_cachedSpectrum.magnitudes[i] + (1.0 - alpha) * data.magnitudes[i];
            }
            m_cachedSpectrum.frequencies = data.frequencies;
        }
    }
}

void MainWindow::refreshPlot() {
    if (!m_hasCachedSpectrum)
        return;

    if (m_spectrogramMode) {
        refreshSpectrogram();
    } else {
        SpectrumData localCopy;

        { QMutexLocker locker(&m_spectrumMutex);
          localCopy = m_cachedSpectrum;
        }

        m_plot->graph(0)->setData(localCopy.frequencies,
                                  localCopy.magnitudes);

        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
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

void MainWindow::onSmoothingChanged(int value) {
    // Convert slider value (0-95) to alpha (0.00-0.95)
    m_smoothingAlpha = value / 100.0;

    // Update label
    m_smoothingLabel->setText(QString("Smoothing: %1").arg(m_smoothingAlpha, 0, 'f', 2));
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    // Keep buttons positioned correctly
    if (m_resetButton && m_plot) {
        m_resetButton->move(m_plot->width() - 35, 5);
    }
    if (m_toggleButton && m_plot) {
        m_toggleButton->move(m_plot->width() - 95, 5);
    }
}

void MainWindow::setupSpectrogram() {
    // Create color map - note: we swap axes for horizontal scrolling
    // X-axis = time (scrolls right to left), Y-axis = frequency
    m_colorMap = new QCPColorMap(m_plot->xAxis, m_plot->yAxis);

    // Set up data dimensions (time columns × frequency bins)
    m_colorMap->data()->setSize(MAX_SPECTROGRAM_ROWS, 512);

    // Set key/value ranges to match cell indices (0-199 for time, 0-511 for frequency bins)
    m_colorMap->data()->setKeyRange(QCPRange(0, MAX_SPECTROGRAM_ROWS - 1));
    m_colorMap->data()->setValueRange(QCPRange(0, 511));

    // Set up color gradient (black -> blue -> green -> yellow -> red)
    QCPColorGradient gradient;
    gradient.setColorStopAt(0.0, QColor(0, 0, 0));       // Black for -80 dB
    gradient.setColorStopAt(0.25, QColor(0, 0, 128));    // Dark blue
    gradient.setColorStopAt(0.5, QColor(0, 255, 0));     // Green
    gradient.setColorStopAt(0.75, QColor(255, 255, 0));  // Yellow
    gradient.setColorStopAt(1.0, QColor(255, 0, 0));     // Red for 0 dB
    m_colorMap->setGradient(gradient);

    // Set data range (-80 to 0 dB)
    m_colorMap->setDataRange(QCPRange(-80, 0));

    // Enable interpolation for smoother display
    m_colorMap->setInterpolate(true);

    // Create color scale but DON'T add it to layout yet
    m_colorScale = new QCPColorScale(m_plot);
    m_colorScale->setType(QCPAxis::atRight);
    m_colorMap->setColorScale(m_colorScale);
    m_colorScale->axis()->setLabel("Magnitude (dB)");
    m_colorScale->axis()->setLabelColor(Qt::white);
    m_colorScale->axis()->setTickLabelColor(Qt::white);
    m_colorScale->axis()->setBasePen(QPen(Qt::white));
    m_colorScale->axis()->setTickPen(QPen(Qt::white));

    // Initialize all cells to -80 dB (black/silent)
    for (int col = 0; col < MAX_SPECTROGRAM_ROWS; ++col) {
        for (int row = 0; row < 512; ++row) {
            m_colorMap->data()->setCell(col, row, -80.0);
        }
    }

    // Initially hide everything
    m_colorMap->setVisible(false);
    m_colorScale->setVisible(false);
}

void MainWindow::refreshSpectrogram() {
    SpectrumData localCopy;

    { QMutexLocker locker(&m_spectrumMutex);
      localCopy = m_cachedSpectrum;
    }

    // Debug: print to verify we're getting data and writing it
    static int debugCount = 0;
    if (debugCount++ < 10) {
        qDebug() << "Spectrogram update" << debugCount << "- magnitudes size:" << localCopy.magnitudes.size();
        if (!localCopy.magnitudes.isEmpty()) {
            qDebug() << "  Sample values:" << localCopy.magnitudes[0] << localCopy.magnitudes[100] << localCopy.magnitudes[200];
            qDebug() << "  ColorMap visible:" << m_colorMap->visible() << "Range:" << m_colorMap->dataRange();
        }
    }

    // Scroll existing data left by one column (past moves left)
    for (int col = 0; col < MAX_SPECTROGRAM_ROWS - 1; ++col) {
        for (int row = 0; row < 512; ++row) {
            double value = m_colorMap->data()->cell(col + 1, row);
            m_colorMap->data()->setCell(col, row, value);
        }
    }

    // Add new spectrum data to rightmost column (present on the right)
    int rightCol = MAX_SPECTROGRAM_ROWS - 1;
    for (int i = 0; i < localCopy.magnitudes.size() && i < 512; ++i) {
        m_colorMap->data()->setCell(rightCol, i, localCopy.magnitudes[i]);
    }

    m_spectrogramRows = qMin(m_spectrogramRows + 1, MAX_SPECTROGRAM_ROWS);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::onToggleDisplayMode() {
    m_spectrogramMode = !m_spectrogramMode;

    if (m_spectrogramMode) {
        // Add color scale to layout when entering spectrogram mode
        m_plot->plotLayout()->addElement(0, 1, m_colorScale);
        m_plot->plotLayout()->setColumnStretchFactor(0, 0.8);
        m_plot->plotLayout()->setColumnStretchFactor(1, 0.2);

        // Switch to spectrogram
        m_plot->graph(0)->setVisible(false);
        m_colorMap->setVisible(true);
        m_colorScale->setVisible(true);
        m_toggleButton->setText("FFT");
        m_smoothingSlider->setEnabled(false);  // Disable smoothing in spectrogram mode

        // Update axes for spectrogram
        m_plot->xAxis->setLabel("Time (updates)");
        m_plot->xAxis->setScaleType(QCPAxis::stLinear);  // Linear for time
        m_plot->xAxis->setRange(0, MAX_SPECTROGRAM_ROWS - 1);

        m_plot->yAxis->setLabel("Frequency Bin");
        m_plot->yAxis->setScaleType(QCPAxis::stLinear);  // Linear bin index
        m_plot->yAxis->setRange(0, 511);
    } else {
        // Switch to spectrum
        m_plot->graph(0)->setVisible(true);
        m_colorMap->setVisible(false);
        m_colorScale->setVisible(false);
        m_toggleButton->setText("Spec");
        m_smoothingSlider->setEnabled(true);

        // Restore axes for spectrum
        m_plot->xAxis->setLabel("Frequency (Hz)");
        m_plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
        m_plot->xAxis->setRange(31.5, 20000);

        m_plot->yAxis->setLabel("Magnitude (dB)");
        m_plot->yAxis->setScaleType(QCPAxis::stLinear);
        m_plot->yAxis->setRange(-80, 0);

        // Remove color scale from layout when exiting spectrogram mode
        m_plot->plotLayout()->take(m_colorScale);
        m_plot->plotLayout()->simplify();
    }

    m_plot->replot();
}
