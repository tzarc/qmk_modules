// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

typedef union __attribute__((packed)) sfdp_dword_t {
    uint32_t u32;
    uint8_t  u8[4];
} sfdp_dword_t;
_Static_assert(sizeof(sfdp_dword_t) == 4, "sfdp_dword_t size is not 4 bytes");

typedef struct __attribute__((packed)) sfdp_header_t {
    uint32_t signature;
    uint8_t  sfdp_minor;
    uint8_t  sfdp_major;
    uint8_t  header_count;
    uint8_t  reserved_0xFF;
} sfdp_header_t;
_Static_assert(sizeof(sfdp_header_t) == 8, "sfdp_header_t size is not 8 bytes");

typedef struct __attribute__((packed)) sfdp_parameter_header_t {
    union {
        sfdp_dword_t a;
        struct __attribute__((packed)) {
            uint8_t jedec_id;
            uint8_t minor;
            uint8_t major;
            uint8_t length;
        };
    };
    union {
        sfdp_dword_t b;
        struct __attribute__((packed)) {
            uint32_t table_pointer : 24;
            uint8_t  reserved_0xFF;
        };
    };
} sfdp_parameter_header_t;
_Static_assert(sizeof(sfdp_parameter_header_t) == 8, "sfdp_parameter_header_t size is not 8 bytes");

typedef union sfdp_flashparam_dword_1_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint8_t erase_size : 2;
        uint8_t write_granularity : 1;
        uint8_t write_enable_insn_required : 1;
        uint8_t write_enable_opcode_select : 1;
        uint8_t reserved_0 : 3;
        uint8_t erase_4kB_opcode;
        uint8_t support_1_1_2_fastread : 1;
        uint8_t address_bytes : 2;
        uint8_t support_dtr_clocking : 1;
        uint8_t support_1_2_2_fastread : 1;
        uint8_t support_1_4_4_fastread : 1;
        uint8_t support_1_1_4_fastread : 1;
        uint8_t reserved_0xFF;
    };
} sfdp_flashparam_dword_1_t;
_Static_assert(sizeof(sfdp_flashparam_dword_1_t) == 4, "sfdp_flashparam_dword_1_t size is not 4 bytes");

typedef union sfdp_flashparam_dword_2_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint32_t density : 31;
        uint8_t  is_high_density : 1;
    };
} sfdp_flashparam_dword_2_t;
_Static_assert(sizeof(sfdp_flashparam_dword_2_t) == 4, "sfdp_flashparam_dword_2_t size is not 4 bytes");

typedef union sfdp_flashparam_dword_3_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint8_t wait_states_1_4_4_fastread : 5;
        uint8_t num_mode_bits_1_4_4_fastread : 3;
        uint8_t read_opcode_1_4_4_fastread;
        uint8_t wait_states_1_1_4_fastread : 5;
        uint8_t num_mode_bits_1_1_4_fastread : 3;
        uint8_t read_opcode_1_1_4_fastread;
    };
} sfdp_flashparam_dword_3_t;
_Static_assert(sizeof(sfdp_flashparam_dword_3_t) == 4, "sfdp_flashparam_dword_3_t size is not 4 bytes");

typedef union sfdp_flashparam_dword_4_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint8_t wait_states_1_1_2_fastread : 5;
        uint8_t num_mode_bits_1_1_2_fastread : 3;
        uint8_t read_opcode_1_1_2_fastread;
        uint8_t wait_states_1_2_2_fastread : 5;
        uint8_t num_mode_bits_1_2_2_fastread : 3;
        uint8_t read_opcode_1_2_2_fastread;
    };
} sfdp_flashparam_dword_4_t;
_Static_assert(sizeof(sfdp_flashparam_dword_4_t) == 4, "sfdp_flashparam_dword_4_t size is not 4 bytes");

typedef union sfdp_flashparam_dword_5_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint8_t  supports_2_2_2_fastread : 1;
        uint32_t reserved_0 : 3;
        uint8_t  supports_4_4_4_fastread : 1;
        uint32_t reserved_1 : 27;
    };
} sfdp_flashparam_dword_5_t;
_Static_assert(sizeof(sfdp_flashparam_dword_5_t) == 4, "sfdp_flashparam_dword_5_t size is not 4 bytes");

typedef union sfdp_flashparam_dword_6_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint32_t reserved_0 : 16;
        uint8_t  wait_states_2_2_2_fastread : 5;
        uint8_t  num_mode_bits_2_2_2_fastread : 3;
        uint8_t  read_opcode_2_2_2_fastread;
    };
} sfdp_flashparam_dword_6_t;
_Static_assert(sizeof(sfdp_flashparam_dword_6_t) == 4, "sfdp_flashparam_dword_6_t size is not 4 bytes");

typedef union sfdp_flashparam_dword_7_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint8_t wait_states_4_4_4_fastread : 5;
        uint8_t num_mode_bits_4_4_4_fastread : 3;
        uint8_t read_opcode_4_4_4_fastread;
    };
} sfdp_flashparam_dword_7_t;
_Static_assert(sizeof(sfdp_flashparam_dword_7_t) == 4, "sfdp_flashparam_dword_7_t size is not 4 bytes");

typedef union sfdp_flashparam_dword_8_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint8_t sector_type_1_size;
        uint8_t sector_type_1_erase_opcode;
        uint8_t sector_type_2_size;
        uint8_t sector_type_2_erase_opcode;
    };
} sfdp_flashparam_dword_8_t;
_Static_assert(sizeof(sfdp_flashparam_dword_8_t) == 4, "sfdp_flashparam_dword_8_t size is not 4 bytes");

typedef union sfdp_flashparam_dword_9_t {
    sfdp_dword_t dword;
    struct __attribute__((packed)) {
        uint8_t sector_type_3_size;
        uint8_t sector_type_3_erase_opcode;
        uint8_t sector_type_4_size;
        uint8_t sector_type_4_erase_opcode;
    };
} sfdp_flashparam_dword_9_t;
_Static_assert(sizeof(sfdp_flashparam_dword_9_t) == 4, "sfdp_flashparam_dword_9_t size is not 4 bytes");
