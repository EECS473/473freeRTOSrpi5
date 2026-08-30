PER_CPU_STACK_EL1_SIZE = 0x100000;
PER_CPU_STACK_EL2_SIZE = 0x010000;
PER_CPU_STACK_EL3_SIZE = 0x004000;
NUM_CORES = 4;

MEMORY {
    RAM : ORIGIN = 0x80000, LENGTH = 0x8000000
}

SECTIONS {
    .text : {
        __text_start = .;
        *(.text.start)
        *(.text)
        *(.text.*)
        __text_end = .;
    } >RAM

    .rodata : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        __rodata_end = .;
    } >RAM

    .data : ALIGN(16) {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    } >RAM

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    } >RAM

    .heap : ALIGN(16) {
        __heap_start = .;
        . += 32M;
        __heap_end = .;
    } >RAM

    .stack : ALIGN(16) {
        __stack_el1_start = .;
        . += PER_CPU_STACK_EL1_SIZE * NUM_CORES;
        __stack_el1_end = .;
    } >RAM

    .stack_el2 : ALIGN(16) {
        __stack_el2_start = .;
        . += PER_CPU_STACK_EL2_SIZE * NUM_CORES;
        __stack_el2_end = .;
    } >RAM

    .stack_el3 : ALIGN(16) {
        __stack_el3_start = .;
        . += PER_CPU_STACK_EL3_SIZE * NUM_CORES;
        __stack_el3_end = .;
    } >RAM

}

__per_cpu_stack_el1_size = PER_CPU_STACK_EL1_SIZE;
__per_cpu_stack_el2_size = PER_CPU_STACK_EL2_SIZE;
__per_cpu_stack_el3_size = PER_CPU_STACK_EL3_SIZE;

__stack_end = __stack_el1_end;
__per_cpu_stack_size = __per_cpu_stack_el1_size;
