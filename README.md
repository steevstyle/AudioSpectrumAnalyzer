# AudioSpectrumAnalyzer
Spectrum analyzer for BeagleBone Black running arm linux with lcd display cape

To use Analog-to-Digital Converter (ADC) on AIN pin, input voltage must be within the range of 0-1.8V
To achieve this, we connected a line-level audio signal (in our case, the line output of a Focusrite Scarlett audio interface) to a simple voltage divider & attenuator circuit with AC coupling.

# SETUP before running
run adc_test on BeagleBone to measure voltage on AIN0. 
Connect AIN0 to VDD_ADC and verify reading is approx. 1.8V. Connect to ground and verify it reads 0V.
Calibrate audio device by adjusting output level with test signal (white noise, sine wave)
