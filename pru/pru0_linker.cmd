MEMORY
{
    PAGE 0:
      PRU_IMEM: ORIGIN = 0x00000000, LENGTH = 0x00002000

    PAGE 1:
      PRU_DMEM: ORIGIN = 0x00000000, LENGTH = 0x00002000
}

SECTIONS
{
    .text : { *(.text*) } > PRU_IMEM, PAGE 0
    
    /* Resource table MUST be first in data memory */
    .resource_table : {
        *(.resource_table)
    } > PRU_DMEM, PAGE 1
    
    .data : { *(.data*) } > PRU_DMEM, PAGE 1
    .bss : { *(.bss*) } > PRU_DMEM, PAGE 1
    .rodata : { *(.rodata*) } > PRU_DMEM, PAGE 1
    .creg.PRU_CFG.noload : { *(.creg*) } > PRU_DMEM, PAGE 1
}
