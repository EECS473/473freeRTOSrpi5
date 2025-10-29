set(CMAKE_SYSTEM_NAME Generic CACHE STRING "")
set(CMAKE_SYSTEM_PROCESSOR aarch64 CACHE STRING "")
set(CMAKE_HOST_SYSTEM_NAME Linux CACHE STRING "")
set(CMAKE_HOST_SYSTEM_PROCESSOR x86_64 CACHE STRING "")

set(TOOLCHAIN_PATH /usr/aarch64-none-elf CACHE PATH "")
set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/bin/aarch64-none-elf-gcc CACHE STRING "")
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/bin/aarch64-none-elf-g++ CACHE STRING "")

set(CMAKE_C_FLAGS "-march=armv8-a -mtune=cortex-a76 -Wall -Wextra" CACHE STRING "C flags")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -fno-rtti" CACHE STRING "C++ flags") 

