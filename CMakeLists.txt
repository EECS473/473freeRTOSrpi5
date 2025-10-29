cmake_minimum_required(VERSION 3.20)

project(raspberrypi_freertos LANGUAGES ASM C CXX)

add_library(freertos_config INTERFACE)

target_include_directories(freertos_config SYSTEM
  INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)


set(FREERTOS_PORT GCC_ARM_AARCH64 CACHE STRING "")

add_subdirectory(FreeRTOS-Kernel)

add_executable(kernel8.elf
  startup.S
  syscall.c
  main.c
  bcm2712_timer.c
  bcm2712_uart10.c
  bcm2712_gpio.c
)

target_include_directories(kernel8.elf
  PRIVATE
    include
)

target_link_options(kernel8.elf
  PRIVATE
    -T ${CMAKE_SOURCE_DIR}/raspberrypi.ld
    -nostartfiles
    # -Wl,--no-warn-rwx-segments
    -Wl,-Map ${CMAKE_BINARY_DIR}/kernel_2712.map
)


add_custom_command(
  OUTPUT kernel_2712.img
  COMMAND ${CMAKE_OBJCOPY} -O binary kernel8.elf kernel_2712.img
  DEPENDS kernel8.elf
)

add_custom_target(img ALL DEPENDS kernel_2712.img)

