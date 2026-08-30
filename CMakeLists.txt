cmake_minimum_required(VERSION 3.20)

project(
  raspberrypi_freertos
  LANGUAGES
    ASM
    C
    CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(APPLICATION_SOURCES main.c)

set(
  CMAKE_EXPORT_COMPILE_COMMANDS
  ON
  CACHE INTERNAL
  "compile_commands.json"
)

add_library(
  freertos_config
  INTERFACE
)

target_include_directories(
  freertos_config
  SYSTEM
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(
  freertos_config
  INTERFACE
    projCOVERAGE_TEST=0
    GUEST
    BCM2712
)

# select heap_1.c no free, configAPPLICATION_ALLOCATED_HEAP=0 allocating ucHeap on .bss for zero inited to avoid memset issue, see task.c memset commented, temporary workaround
set(
  FREERTOS_HEAP
  "1"
  CACHE STRING
  ""
  FORCE
)

set(
  FREERTOS_PORT
  GCC_ARM_AARCH64
  CACHE STRING
  ""
  FORCE
)

add_subdirectory(
  FreeRTOS-Kernel
)

#
# ---------------------------------------------------------------------------
# BCM2712 / RP1 hardware support
# ---------------------------------------------------------------------------
#

add_library(
  bcm2712_driver
  STATIC

    drivers/bcm2712/bcm2712_timer.c
    drivers/bcm2712/bcm2712_smp.c

    drivers/rp1/rp1_uart0.c

    #
    # Raspberry Pi 5 southbridge support.
    #
    drivers/rp1/rp1.c

    drivers/rpi4_compat/gpio.c
    drivers/rpi4_compat/interrupt.c
)

target_link_libraries(
  bcm2712_driver
  PUBLIC
    freertos_config
    freertos_kernel_include
    freertos_kernel_port_headers
)

target_include_directories(
  bcm2712_driver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/bcm2712
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rp1
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rpi4_compat
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/aarch64
)

#
# ---------------------------------------------------------------------------
# Kernel ELF
# ---------------------------------------------------------------------------
#

add_executable(
  kernel8.elf

    platform/aarch64/startup.S
    platform/aarch64/aarch64_memset.S
    platform/aarch64/aarch64_mmu.c
    platform/aarch64/syscall.c
    drivers/rpi4_compat/platform_runtime.c
    ${APPLICATION_SOURCES}
)

target_link_options(kernel8.elf PRIVATE -Wl,--wrap=main)

target_link_options(
  kernel8.elf
  PRIVATE
    -T
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/aarch64/raspberrypi.ld

    -nostartfiles

    -Wl,--no-warn-rwx-segments
    -Wl,-Map
    ${CMAKE_BINARY_DIR}/kernel_2712.map
)

#
# The AArch64 FreeRTOS port depends on the platform driver because the
# FreeRTOS tick uses bcm2712_timer.c.
#
target_link_libraries(
  freertos_kernel_port
  PUBLIC
    bcm2712_driver
)

target_link_libraries(
  kernel8.elf
  PRIVATE
    freertos_kernel
    bcm2712_driver
)

add_custom_command(
  TARGET kernel8.elf
  POST_BUILD
  COMMAND
    ${CMAKE_OBJCOPY}
    -O binary
    $<TARGET_FILE:kernel8.elf>
    ${CMAKE_BINARY_DIR}/kernel_2712.img
  BYPRODUCTS
    ${CMAKE_BINARY_DIR}/kernel_2712.img
  COMMENT
    "Creating kernel_2712.img"
  VERBATIM
)
