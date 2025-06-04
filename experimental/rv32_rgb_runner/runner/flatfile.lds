__heap_size = 256;
__stack_size = 256;

MEMORY
{
    TEXT (rwx) : ORIGIN = 0x80000000, LENGTH = 2048  # 2kB total for code + RAM
}

ENTRY(_load)

SECTIONS
{
    .text : ALIGN(4) {
        __TEXT_BEGIN__ = .;
        KEEP(*(.runner_base))
        KEEP(*(.runner_base.*))
        KEEP(*(.thunks.*))
        *(.entry.text)
        *(.init.literal)
        *(.init)
        *(.text)
        *(.literal .text .literal.* .text.* .stub)
        *(.out_jump.literal.*)
        *(.out_jump.*)
        __TEXT_END__ = .;

        . = ALIGN(4);

        __preinit_array_start = .;
        KEEP (*(SORT(.preinit_array.*)))
        KEEP (*(.preinit_array*))
        __preinit_array_end = .;

        . = ALIGN(4);

        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        __init_array_end = .;

        . = ALIGN(4);

        __fini_array_start = .;
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        __fini_array_end = .;

        . = ALIGN(4);

        __DATA_BEGIN__ = .;
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        *(.rodata1)
        *(.dynsbss)
        *(.gnu.linkonce.sb.*)
        *(.scommon)
        *(.gnu.linkonce.sb2.*)
        *(.sbss)
        *(.sbss.*)
        *(.sbss2)
        *(.sbss2.*)
        *(.dynbss)
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
        *(.got)
        *(.got.*)
        __DATA_END__ = .;
    } >TEXT

    .bss : ALIGN(4) {
        __BSS_BEGIN__ = .;
        *(.bss)
        *(.bss.*)
        __BSS_END__ = .;
    } >TEXT

    .heap : ALIGN(4) {
        _sheap = .;
        . = . + __heap_size;
        _eheap = .;
    } >TEXT

    .stack : ALIGN(4) {
        _estack = .;
        . = . + __stack_size;
        _sstack = .;
    } >TEXT

    /DISCARD/ :
    {
        *(.interp)
        *(.dynsym)
        *(.dynstr)
        *(.header)
    } : phdr
}
