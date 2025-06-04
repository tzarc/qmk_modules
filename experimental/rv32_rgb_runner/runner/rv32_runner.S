# vim: set ft=asm ts=8 sw=8 expandtab
# Copyright 2024 Nick Brassel (@tzarc)
# SPDX-License-Identifier: GPL-2.0-or-later
.section .runner_base
.global _start

.align 4
        .long _sstack                   # First 4 bytes of the data represent the total amount of RAM required

_start:
        la t3, _api_table_end           # end of table
        la t2, _api_table               # start of table
        li t1, 4                        # index multiplier
        mul t0, t0, t1                  # multiply supplied index (t0) by 4 to get memory offset
        add t0, t0, t2                  # add memory offset to start of table to get function to invoke
        bge t0, t3, _shutdown           # exit if we've calculated past the end of the table
        lw t0, 0(t0)                    # load the address from the table
        beq t0, x0, _shutdown           # exit if the resulting location is null (weak, undefined in code)
        la sp, _sstack                  # set up stack
        addi sp,sp,-16                  # make stack space
        sw ra,12(sp)                    # save return address (not that it's in use...)
        jalr t0                         # invoke the function from the table

_shutdown:
        li a7, 0                        # load 0 (RV32_EXIT) as ecall param
        ecall                           # kick off termination procedure
        j _shutdown                     # Keep retying shutdown if there was a failure for whatever reason

.global _api_table

.weak ctors_thunk
.weak dtors_thunk
.weak effect_init_thunk
.weak effect_begin_iter_thunk
.weak effect_led_thunk
.weak effect_end_iter_thunk

_api_table:                             # API table for functions to invoke -- kicking off the core with t0 set to an index will invoke the corresponding API
        .long ctors_thunk
        .long dtors_thunk
        .long effect_init_thunk
        .long effect_begin_iter_thunk
        .long effect_led_thunk
        .long effect_end_iter_thunk

_api_table_end:
