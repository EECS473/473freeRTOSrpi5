cmake_minimum_required(VERSION 3.20)

project(raspberrypi_freertos LANGUAGES C CXX)


set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_SOURCE_DIR}/FreeRTOS-Kernel/examples/template_configuration/)
set(FREERTOS_PORT GCC_ARM_AARCH64)
add_subdirectory(FreeRTOS-Kernel)
