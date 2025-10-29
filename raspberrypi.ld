MEMORY {
    RAM : ORIGIN = 0x80000, LENGTH = 0x400000
}

SECTIONS {
    .text : {
        __text_start = .;
        *(.text.start)
        *(.text)
        *(.text.*)
        __text_end = .;
    } >RAM

    .vector : {
        __vector_start = .;
        *(.vector)
        *(.vector.*)
        . += 0x400;
        __vector_end = .;
    } >RAM

    .rodata : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        __rodata_end = .;
    } >RAM

    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    } >RAM

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    } >RAM

    .heap : {
        __heap_start = .;
        . += 1M;
        __heap_end = .;
    } >RAM

    .stack : ALIGN(16) {
        __stack_start = .;
        . += 1M;
        __stack_end = .;
    } >RAM

    .stack_el2 : ALIGN(16) {
        __stack_el2_start = .;
        . += 1M;
        __stack_el2_end = .;
    } >RAM

}

