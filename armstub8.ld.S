INCLUDE armstub8.memory

SECTIONS {
    .hdr : {
        __hdr_start = .;
        *(.hdr.*)
        __hdr_end = .;
    } >REGION_HEADER

    .text : {
        __text_start = .;
        *(.armstub8.*)
        *(.text.start)
        *(.text)
        *(.text.*)
        __text_end = .;
    } >REGION_TEXT

    .rodata : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        __rodata_end = .;
    } >REGION_RODATA

    .data : ALIGN(16) {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    } >REGION_DATA

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    } >REGION_BSS

    .stack (NOLOAD) : ALIGN(16) {
        __stack_start = .;
        . += 0x40000;
        __stack_end = .;
    } >REGION_STACK
}

