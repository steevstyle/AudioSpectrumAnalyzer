# PRU C Compiler toolchain
PRU_CGT = /home/stopkins/ti-cgt-pru_2.3.3
CLPRU = clpru
LNKPRU = clpru

# Include paths
INCLUDE = --include_path=$(PRU_CGT)/include --include_path=include

# Compiler flags
CFLAGS = -v3 --hardware_mac=on

# Linker flags
LDFLAGS = -z -cr --ram_model -i$(PRU_CGT)/lib

# Target
TARGET = pru_adc_sampler.out

# Source files
C_SOURCES = pru_adc_sampler.c
ASM_SOURCES = delay.asm

# Object files
C_OBJS = $(C_SOURCES:.c=.obj)
ASM_OBJS = $(ASM_SOURCES:.asm=.obj)
ALL_OBJS = $(C_OBJS) $(ASM_OBJS)

# Linker command file
LINKER_CMD = pru0_linker.cmd

all: $(TARGET)

# Compile C source to object file
%.obj: %.c
	$(CLPRU) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Compile assembly source to object file
%.obj: %.asm
	$(CLPRU) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Link object files to create output
$(TARGET): $(ALL_OBJS) $(LINKER_CMD)
	$(LNKPRU) $(LDFLAGS) $(ALL_OBJS) \
		-m $(TARGET:.out=.map) \
		$(LINKER_CMD) \
		-o $(TARGET)

clean:
	rm -f *.obj *.out *.map

.PHONY: all clean
