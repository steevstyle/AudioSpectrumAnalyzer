# AudioSpectrumAnalyzer

Real-time audio spectrum analyzer for BeagleBone Black with PRU-based ADC sampling and Qt5 GUI. Features both FFT spectrum and spectrogram (waterfall) display modes with adjustable exponential smoothing.

## Features

- **48 kHz sampling rate** via PRU0 using IEP timer for cycle-accurate timing
- **1024-point FFT** with Hann windowing and dB normalization
- **Dual display modes**: FFT spectrum graph and scrolling spectrogram
- **Adjustable smoothing**: Exponential moving average (0.0-0.95)
- **Efficient rendering**: Circular buffer design for smooth 46 Hz update rate
- **Hardware accelerated**: PRU handles ADC sampling, ARM handles DSP/GUI

## Hardware Requirements

- BeagleBone Black (AM335x SoC)
- LCD display cape (for Qt5 GUI)
- Audio input circuit with voltage divider/attenuator
  - Input range: 0-1.8V (ADC limitation)
  - AC coupling recommended for line-level audio sources

## Software Dependencies

### 1. TI PRU Code Generation Tools v2.3.3

Download from [TI's website](https://www.ti.com/tool/PRU-CGT):

```bash
cd ~
wget https://software-dl.ti.com/codegen/esd/cgt_public_sw/PRU/2.3.3/ti_cgt_pru_2.3.3_linux_installer_x86.bin
chmod +x ti_cgt_pru_2.3.3_linux_installer_x86.bin
./ti_cgt_pru_2.3.3_linux_installer_x86.bin
# Install to ~/ti-cgt-pru_2.3.3
```

Add to PATH in `~/.bashrc`:

```bash
export PATH="$HOME/ti-cgt-pru_2.3.3/bin:$PATH"
```

Then source: `source ~/.bashrc`

### 2. FFTW3 Library

**Option A: Cross-compile on host machine (recommended)**

```bash
wget http://www.fftw.org/fftw-3.3.10.tar.gz
tar -xzf fftw-3.3.10.tar.gz
cd fftw-3.3.10

./configure --host=arm-linux-gnueabihf \
    --prefix=/path/to/AudioSpectrumAnalyzer/fftw-arm \
    --enable-float \
    CC=arm-linux-gnueabihf-gcc

make -j4
make install
```

**Option B: Compile natively on BeagleBone**

```bash
wget http://www.fftw.org/fftw-3.3.10.tar.gz
tar -xzf fftw-3.3.10.tar.gz
cd fftw-3.3.10

./configure --prefix=$HOME/fftw-arm --enable-float
make -j2
make install
```

### 3. Qt5 Development Libraries

On BeagleBone Black:

```bash
sudo apt-get update
sudo apt-get install -y \
    qt5-default \
    qtbase5-dev \
    libqt5widgets5 \
    libqt5gui5 \
    libqt5core5a
```

## Project Structure

```
AudioSpectrumAnalyzer/
├── pru/
│   ├── pru_adc_sampler.c       # PRU firmware for 48kHz ADC sampling
│   ├── delay.asm                # Assembly delay routine
│   ├── pru0_linker.cmd          # Linker script
│   ├── include/
│   │   ├── pru_cfg.h
│   │   └── resource_table_pru0.h
│   └── Makefile                 # PRU firmware build system
├── fftw-arm/                    # FFTW3 library (local install)
│   ├── include/
│   └── lib/
├── mainwindow.cpp/h             # Qt5 main window with spectrum/spectrogram
├── dspthread.cpp/h              # DSP thread (FFT computation)
├── spectrumdata.h               # Data structure for spectrum
├── qcustomplot.cpp/h            # Plotting library
├── main.cpp                     # Qt application entry
└── AudioSpectrumAnalyzer.pro    # Qt project file
```

## Building

### 1. Build PRU Firmware

```bash
cd pru
make clean
make
```

This produces `pru_adc_sampler.out` which can be loaded via remoteproc.

**Note**: The Makefile expects TI tools at `~/ti-cgt-pru_2.3.3`. If installed elsewhere, update the `PRU_CGT` variable in `pru/Makefile`.

### 2. Build Qt Application

Update `AudioSpectrumAnalyzer.pro` to point to your FFTW installation:

```qmake
INCLUDEPATH += /path/to/AudioSpectrumAnalyzer/fftw-arm/include
LIBS += -L/path/to/AudioSpectrumAnalyzer/fftw-arm/lib -lfftw3
```

Then build:

```bash
qmake AudioSpectrumAnalyzer.pro
make -j4
```

## Running

### 1. Load PRU Firmware

Copy firmware to remoteproc directory:

```bash
sudo cp pru/pru_adc_sampler.out /lib/firmware/am335x-pru0-fw
```

Stop and restart PRU0:

```bash
echo 'stop' | sudo tee /sys/class/remoteproc/remoteproc1/state
echo 'start' | sudo tee /sys/class/remoteproc/remoteproc1/state
```

Verify PRU is running:

```bash
cat /sys/class/remoteproc/remoteproc1/state
# Should show "running"
```

### 2. Launch Qt Application

```bash
./AudioSpectrumAnalyzer
```

**Controls**:
- **Toggle button**: Switch between FFT spectrum and spectrogram modes
- **Smoothing slider**: Adjust exponential averaging (0.0 = no smoothing, 0.95 = heavy smoothing)
- **X button** (top-right): Reset display / clear accumulated data

## Hardware Setup & Calibration

To use Analog-to-Digital Converter (ADC) on AIN pin, input voltage must be within the range of 0-1.8V. To achieve this, we connected a line-level audio signal (in our case, the line output of a Focusrite Scarlett audio interface) to a simple voltage divider & attenuator circuit with AC coupling.

### Before Running

1. Run `adc_test` on BeagleBone to measure voltage on AIN0
2. Connect AIN0 to VDD_ADC and verify reading is approx. 1.8V
3. Connect to ground and verify it reads 0V
4. Calibrate audio device by adjusting output level with test signal (white noise, sine wave)

## Technical Details

### PRU ADC Sampling

- **Sample rate**: 48 kHz (200 MHz / 4167 cycles)
- **Resolution**: 12-bit (0-4095)
- **Voltage range**: 0.0-1.8V
- **Buffering**: Ping-pong (dual 1024-sample buffers)
- **Timing**: IEP timer for cycle-accurate sampling
- **Shared memory**: 0x4A310000 (ARM view), 0x00010000 (PRU view)
  - Buffer A: offset 0x0000
  - Buffer B: offset 0x0800
  - Ready flag: offset 0x1000

### DSP Processing

- **FFT size**: 1024 points
- **Window**: Hann (coherent gain = 0.5)
- **Transform**: FFTW r2r (real to half-complex)
- **Normalization**: dB relative to 0.9V full-scale
- **DC removal**: Mean subtraction before windowing
- **Smoothing**: Exponential moving average (adjustable α)

### Display Modes

**FFT Spectrum**:
- X-axis: Logarithmic frequency (20 Hz - 24 kHz)
- Y-axis: Magnitude (dB)
- Update rate: 30 Hz (timer-based refresh)

**Spectrogram**:
- X-axis: Time (24 seconds circular buffer)
- Y-axis: Frequency (linear, 46.875 Hz bins)
- Color scale: -75 to -25 dB (gpJet gradient)
- Update rate: ~46 Hz (processes every PRU buffer)
- Rendering: Circular buffer (no scrolling overhead)

## Troubleshooting

### PRU not starting

```bash
# Check PRU status
cat /sys/class/remoteproc/remoteproc1/state

# Check kernel logs
dmesg | grep pru
dmesg | grep remoteproc
```

### Display shows "Buffer flag stuck" warning

This indicates PRU firmware is not running. Verify PRU was loaded correctly (see above).

### No FFTW library found

Ensure `LIBS` path in `.pro` file matches your FFTW installation. Try:

```bash
export LD_LIBRARY_PATH=/path/to/fftw-arm/lib:$LD_LIBRARY_PATH
```

### Qt application won't start

Check Qt5 installation:

```bash
qmake --version
ldd ./AudioSpectrumAnalyzer  # Check missing libraries
```

### Compilation errors in pru/

Verify TI toolchain is in PATH:

```bash
which clpru
# Should show: /home/yourusername/ti-cgt-pru_2.3.3/bin/clpru
```

## Performance Notes

- **Update rate**: ~46.875 Hz (1024 samples at 48 kHz = 21.33 ms/buffer)
- **Display refresh**: 30 Hz for FFT mode (reduces CPU load)
- **Circular buffer**: Eliminates expensive scrolling in spectrogram mode
- **Thread separation**: QThread for DSP, main thread for GUI rendering

## License

Educational project for EC535 - Embedded Systems course.
