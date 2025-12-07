#ifndef _RSC_TABLE_PRU0_H_
#define _RSC_TABLE_PRU0_H_

#include <stddef.h>
#include <rsc_types.h>

/* PRU0 Resource Table */
struct pru0_resource_table {
    struct resource_table base;
    uint32_t offset[1];
    struct fw_rsc_carveout shared_ram;
};

/* Place resource table in special section */
#pragma DATA_SECTION(resource_table, ".resource_table")
#pragma RETAIN(resource_table)
struct pru0_resource_table resource_table = {
    .base = {
        1,      // version
        1,      // number of entries
        {0, 0}, // reserved
    },
    .offset = {
        offsetof(struct pru0_resource_table, shared_ram),
    },
    .shared_ram = {
        TYPE_CARVEOUT,
        0,           // device address
        0x00010000,  // physical start of shared RAM
        0x2000,      // size (8KB for buffers + flags)
        0,           // flags
        0,           // reserved
        "PRU_SHARED_RAM"
    }
};

#endif /* _RSC_TABLE_PRU0_H_ */
