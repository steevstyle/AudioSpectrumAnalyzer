# Path to pasm on Signals machines
PASM=$(HOME)/am335x_pru_package/pru_sw/utils/pasm

all: pru_adc.bin

pru_adc.bin: pru_adc.p
	$(PASM) -b pru_adc.p

clean:
	rm -f pru_adc.bin *.lst

install: pru_adc.bin
	# Transfer binary to BeagleBone
