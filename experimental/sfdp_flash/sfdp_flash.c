// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H

#ifdef FLASH_DRIVER_SPI

#    include <stdbool.h>
#    include <stdint.h>
#    include "spi_master.h"
#    include "flash_spi.h"
#    include "sfdp_flash.h"
#    include "sfdp_flash_params.h"

#    ifndef CMD_GET_JEDEC_ID
#        define CMD_GET_JEDEC_ID 0x9F
#    endif

#    ifndef CMD_ENTER_SFDP_MODE
#        define CMD_ENTER_SFDP_MODE 0x5A
#    endif

#    ifndef DUMMY_DATA
#        define DUMMY_DATA 0xFF
#    endif

typedef struct sfdp_runtime_t {
    bool was_checked;
    bool is_supported;
    bool supports_1_1_2_fastread;
    bool supports_1_2_2_fastread;
    bool supports_1_4_4_fastread;
    bool supports_1_1_4_fastread;
    bool supports_2_2_2_fastread;
    bool supports_4_4_4_fastread;
} sfdp_runtime_t;

sfdp_runtime_t sfdp;

static bool spi_flash_start(void) {
    return spi_start(EXTERNAL_FLASH_SPI_SLAVE_SELECT_PIN, EXTERNAL_FLASH_SPI_LSBFIRST, EXTERNAL_FLASH_SPI_MODE, EXTERNAL_FLASH_SPI_CLOCK_DIVISOR);
}

static bool read_jedec_id(uint32_t *id) {
    if (!spi_flash_start()) {
        return false;
    }
    spi_status_t status = spi_write(CMD_GET_JEDEC_ID);
    if (status < 0) {
        sfdp_dprintf("JEDEC ID command failed\n");
        spi_stop();
        return false;
    }

    uint8_t jedec_id[3];
    status = spi_receive(jedec_id, sizeof(jedec_id));
    if (status < 0) {
        sfdp_dprintf("JEDEC ID receive failed\n");
        spi_stop();
        return false;
    }

    spi_stop();

    *id = (jedec_id[0] << 16) | (jedec_id[1] << 8) | jedec_id[2];

    return true;
}

static bool read_sfdp_data(uint32_t addr, void *data, size_t length) {
    uint8_t cmd[] = {
        CMD_ENTER_SFDP_MODE, // enter SFDP mode, deasserting CS will exit
        (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, (addr >> 0) & 0xFF, DUMMY_DATA,
    };

    if (!spi_flash_start()) {
        return false;
    }
    spi_status_t status = spi_transmit(cmd, sizeof(cmd));
    if (status < 0) {
        spi_stop();
        return false;
    }
    status = spi_receive(data, length);
    if (status < 0) {
        spi_stop();
        return false;
    }
    spi_stop();
    return true;
}

bool sfdp_parse_parameter_table(uint32_t table_pointer, size_t length) {
    for (size_t n = 1; n <= length; ++n) {
        union {
            sfdp_dword_t              d;
            sfdp_flashparam_dword_1_t p1;
            sfdp_flashparam_dword_2_t p2;
            sfdp_flashparam_dword_3_t p3;
            sfdp_flashparam_dword_4_t p4;
            sfdp_flashparam_dword_5_t p5;
            sfdp_flashparam_dword_6_t p6;
            sfdp_flashparam_dword_7_t p7;
            sfdp_flashparam_dword_8_t p8;
            sfdp_flashparam_dword_9_t p9;
        } param;
        _Static_assert(sizeof(param) == 4, "param size is not 4 bytes");

        bool ok = read_sfdp_data(table_pointer + ((n - 1) * sizeof(sfdp_dword_t)), (void *)&param, sizeof(param));
        if (!ok) {
            sfdp_dprintf("flash parameter %d unavailable\n", (int)n);
            return false;
        }
        sfdp_dprintf("Flash Parameter %d: 0x%08lX\n", (int)n, param.d.u32);
        switch (n) {
            case 1: {
                sfdp_dprintf("- Erase size: %d, write granularity: %d, SR write enable insn required: %d, SR write enable opcode: 0x%02X\n",
                             (int)(param.p1.erase_size == 1 ? 4096 : 0),              // other states are reserved or unavailable
                             (int)(param.p1.write_granularity ? 64 : 1),              // in bytes
                             (int)param.p1.write_enable_insn_required,                // affects next field
                             (int)(param.p1.write_enable_opcode_select ? 0x06 : 0x50) // dependent on previous field
                );
                sfdp_dprintf("- Erase 4kB opcode: 0x%02X, support 1-1-2 fastread: %d, address bytes: %d, support DTR clocking: %d\n", (int)param.p1.erase_4kB_opcode, (int)param.p1.support_1_1_2_fastread, (int)param.p1.address_bytes, (int)param.p1.support_dtr_clocking);
                sfdp_dprintf("- Support 1-2-2 fastread: %d, support 1-4-4 fastread: %d, support 1-1-4 fastread: %d\n", (int)param.p1.support_1_2_2_fastread, (int)param.p1.support_1_4_4_fastread, (int)param.p1.support_1_1_4_fastread);
                sfdp.supports_1_1_2_fastread = param.p1.support_1_1_2_fastread;
                sfdp.supports_1_2_2_fastread = param.p1.support_1_2_2_fastread;
                sfdp.supports_1_4_4_fastread = param.p1.support_1_4_4_fastread;
                sfdp.supports_1_1_4_fastread = param.p1.support_1_1_4_fastread;
            } break;
            case 2: {
                uint32_t bits   = ((param.p2.is_high_density) ? (1 << (param.p2.density)) : (param.p2.density + 1));
                uint32_t bytes  = bits / 8;
                uint32_t kbytes = bytes / 1024;
                (void)kbytes;
                sfdp_dprintf("- Memory density: %d bits (%d bytes, %d kB)\n", (int)bits, (int)bytes, (int)kbytes);
            } break;
            case 3: {
                if (sfdp.supports_1_1_4_fastread) {
                    sfdp_dprintf("- 1-1-4 fastread wait states: %d, mode bits: %d, read opcode: 0x%02X\n", (int)param.p3.wait_states_1_1_4_fastread, (int)param.p3.num_mode_bits_1_1_4_fastread, (int)param.p3.read_opcode_1_1_4_fastread);
                } else {
                    sfdp_dprintf("- 1-1-4 fastread not supported\n");
                }
                if (sfdp.supports_1_4_4_fastread) {
                    sfdp_dprintf("- 1-4-4 fastread wait states: %d, mode bits: %d, read opcode: 0x%02X\n", (int)param.p3.wait_states_1_4_4_fastread, (int)param.p3.num_mode_bits_1_4_4_fastread, (int)param.p3.read_opcode_1_4_4_fastread);
                } else {
                    sfdp_dprintf("- 1-4-4 fastread not supported\n");
                }
            } break;
            case 4: {
                if (sfdp.supports_1_1_2_fastread) {
                    sfdp_dprintf("- 1-1-2 fastread wait states: %d, mode bits: %d, read opcode: 0x%02X\n", (int)param.p4.wait_states_1_1_2_fastread, (int)param.p4.num_mode_bits_1_1_2_fastread, (int)param.p4.read_opcode_1_1_2_fastread);
                } else {
                    sfdp_dprintf("- 1-1-2 fastread not supported\n");
                }
                if (sfdp.supports_1_2_2_fastread) {
                    sfdp_dprintf("- 1-2-2 fastread wait states: %d, mode bits: %d, read opcode: 0x%02X\n", (int)param.p4.wait_states_1_2_2_fastread, (int)param.p4.num_mode_bits_1_2_2_fastread, (int)param.p4.read_opcode_1_2_2_fastread);
                } else {
                    sfdp_dprintf("- 1-2-2 fastread not supported\n");
                }
            } break;
            case 5: {
                sfdp_dprintf("- Supports 2-2-2 fastread: %d\n", (int)param.p5.supports_2_2_2_fastread);
                sfdp_dprintf("- Supports 4-4-4 fastread: %d\n", (int)param.p5.supports_4_4_4_fastread);
                sfdp.supports_2_2_2_fastread = param.p5.supports_2_2_2_fastread;
                sfdp.supports_4_4_4_fastread = param.p5.supports_4_4_4_fastread;
            } break;
            case 6: {
                if (sfdp.supports_2_2_2_fastread) {
                    sfdp_dprintf("- 2-2-2 fastread wait states: %d, mode bits: %d, read opcode: 0x%02X\n", (int)param.p6.wait_states_2_2_2_fastread, (int)param.p6.num_mode_bits_2_2_2_fastread, (int)param.p6.read_opcode_2_2_2_fastread);
                } else {
                    sfdp_dprintf("- 2-2-2 fastread not supported\n");
                }
            } break;
            case 7: {
                if (sfdp.supports_4_4_4_fastread) {
                    sfdp_dprintf("- 4-4-4 fastread wait states: %d, mode bits: %d, read opcode: 0x%02X\n", (int)param.p7.wait_states_4_4_4_fastread, (int)param.p7.num_mode_bits_4_4_4_fastread, (int)param.p7.read_opcode_4_4_4_fastread);
                } else {
                    sfdp_dprintf("- 4-4-4 fastread not supported\n");
                }
            } break;
            case 8: {
                sfdp_dprintf("- Sector type 1 size: %d, erase opcode: 0x%02X\n", (int)(1 << param.p8.sector_type_1_size), (int)param.p8.sector_type_1_erase_opcode);
                sfdp_dprintf("- Sector type 2 size: %d, erase opcode: 0x%02X\n", (int)(1 << param.p8.sector_type_2_size), (int)param.p8.sector_type_2_erase_opcode);
            } break;
            case 9: {
                sfdp_dprintf("- Sector type 3 size: %d, erase opcode: 0x%02X\n", (int)(1 << param.p9.sector_type_3_size), (int)param.p9.sector_type_3_erase_opcode);
                sfdp_dprintf("- Sector type 4 size: %d, erase opcode: 0x%02X\n", (int)(1 << param.p9.sector_type_4_size), (int)param.p9.sector_type_4_erase_opcode);
            } break;
        }
    }
    return true;
}

bool sfdp_init(void) {
    if (!sfdp.was_checked) {
        // sfdp.was_checked = true;
        spi_init();

        uint32_t jedec_id;
        bool     ok = read_jedec_id(&jedec_id);
        if (!ok) {
            sfdp_dprintf("JEDEC ID unavailable\n");
            return false;
        }
        sfdp_dprintf("JEDEC ID: 0x%06lX\n", jedec_id);

        sfdp_header_t sfdp_header;
        ok = read_sfdp_data(0, (void *)&sfdp_header, sizeof(sfdp_header));
        if (!ok || sfdp_header.reserved_0xFF != 0xFF) {
            sfdp_dprintf("header unavailable\n");
            return false;
        }
        sfdp_dprintf("Signature: 0x%08lX, SFDP rev %d.%d, header count: %d\n", sfdp_header.signature, (int)sfdp_header.sfdp_major, (int)sfdp_header.sfdp_minor, (int)(sfdp_header.header_count + 1));

        if (sfdp_header.signature != 0x50444653) { // "SFDP"
            sfdp_dprintf("not supported\n");
            return false;
        }

        for (size_t n = 0; n < (size_t)(sfdp_header.header_count + 1); ++n) {
            sfdp_parameter_header_t param;
            ok = read_sfdp_data(sizeof(sfdp_header) + (n * sizeof(sfdp_parameter_header_t)), (void *)&param, sizeof(param));
            if (!ok || param.reserved_0xFF != 0xFF) {
                sfdp_dprintf("SFDP parameter header %d unavailable\n", (int)n);
                return false;
            }
            sfdp_dprintf("Parameter header %d A: 0x%08lX, B: 0x%08lX\n", (int)n, param.a.u32, param.b.u32);
            sfdp_dprintf("- JEDEC ID: 0x%02X, param rev %d.%d, parameter length: %d\n", (int)param.jedec_id, (int)param.major, (int)param.minor, (int)param.length);
            sfdp_dprintf("- Parameter table pointer: 0x%08lX\n", (uint32_t)(param.table_pointer));

            if (n == 0) { // Limited to base JEDEC standard parameters
                if (!sfdp_parse_parameter_table(param.table_pointer, param.length)) {
                    return false;
                }
            }
        }
        sfdp.is_supported = true;
    }

    return sfdp.is_supported;
}

bool process_record_sfdp_flash(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_sfdp_flash_kb(keycode, record)) {
        return false;
    }
    switch (keycode) {
        case KC_SFDP: {
            if (record->event.pressed) {
                sfdp_init();
            }
            break;
        }
    }
    return true;
}

#endif // FLASH_DRIVER_SPI
