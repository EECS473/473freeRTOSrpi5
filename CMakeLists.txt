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

# select heap_1.c no free, configAPPLICATION_ALLOCATED_HEAP=0 allocating ucHeap on .bss
set(FREERTOS_HEAP "1" CACHE STRING "" FORCE)
set(FREERTOS_PORT GCC_ARM_AARCH64 CACHE STRING "" FORCE)

add_subdirectory(FreeRTOS-Kernel)

add_library(bcm2712_driver STATIC
  bcm2712_timer.c
  bcm2712_uart10.c
  bcm2712_gpio.c
)

target_include_directories(bcm2712_driver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)


# kernel image
add_executable(kernel8.elf
  startup.S
  syscall.c
  main.c
)

target_link_options(kernel8.elf
  PRIVATE
    -T ${CMAKE_CURRENT_SOURCE_DIR}/raspberrypi.ld
    -nostartfiles
    -Wl,--no-warn-rwx-segments
    -Wl,-Map ${CMAKE_BINARY_DIR}/kernel8.map
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


# armstub8-2712.bin
add_executable(armstub8.elf
  armstub8.S
)

target_link_options(armstub8.elf
  PRIVATE
    -T ${CMAKE_CURRENT_SOURCE_DIR}/armstub8.ld.S
    -nostartfiles
    -nostdlib
    -Wl,--no-warn-rwx-segments
    -Wl,-Map ${CMAKE_BINARY_DIR}/armstub8.map
)

target_link_libraries(armstub8.elf
  PRIVATE
    bcm2712_driver
)

target_link_directories(armstub8.elf
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)


# binary generation
add_custom_command(
  OUTPUT kernel_2712.img armstub8-2712.bin
  COMMAND ${CMAKE_OBJCOPY} -O binary kernel8.elf kernel_2712.img
  COMMAND ${CMAKE_OBJCOPY} -O binary armstub8.elf armstub8-2712.bin
  DEPENDS kernel8.elf armstub8.elf
)

add_custom_target(img ALL DEPENDS kernel_2712.img armstub8-2712.bin)
