cmake_minimum_required(VERSION 3.20)

project(raspberrypi_freertos LANGUAGES ASM C CXX)

add_library(freertos_config INTERFACE)

target_include_directories(freertos_config SYSTEM
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(freertos_config
  INTERFACE
    projCOVERAGE_TEST=0
    GUEST
)

# select heap_1.c no free, configAPPLICATION_ALLOCATED_HEAP=0 allocating ucHeap on .bss for zero inited to avoid memset issue, see task.c memset commented, temporary workaround
set(FREERTOS_HEAP "1" CACHE STRING "" FORCE)
set(FREERTOS_PORT GCC_ARM_AARCH64 CACHE STRING "" FORCE)

add_subdirectory(FreeRTOS-Kernel)

add_library(bcm2712_driver STATIC
  bcm2712_timer.c
  bcm2712_uart10.c
  bcm2712_gpio.c
  bcm2712_smp.c
)

target_link_libraries(bcm2712_driver
  PUBLIC
    freertos_config
    freertos_kernel_include
    freertos_kernel_port_headers
)

target_include_directories(bcm2712_driver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(kernel8.elf
  startup.S
  aarch64_memset.S
  aarch64_mmu.c
  syscall.c
  main.c
)

target_link_options(kernel8.elf
  PRIVATE
    -T ${CMAKE_CURRENT_SOURCE_DIR}/raspberrypi.ld
    -nostartfiles
    -Wl,--no-warn-rwx-segments
    -Wl,-Map ${CMAKE_BINARY_DIR}/kernel_2712.map
)

target_link_libraries(freertos_kernel_port
  PUBLIC
    bcm2712_driver
)

target_link_libraries(kernel8.elf
  PRIVATE
    freertos_kernel
    bcm2712_driver
)


add_custom_command(
  OUTPUT kernel_2712.img
  COMMAND ${CMAKE_OBJCOPY} -O binary kernel8.elf kernel_2712.img
  DEPENDS kernel8.elf
)

add_custom_target(img ALL DEPENDS kernel_2712.img)

