set(CMAKE_SYSTEM_NAME Generic CACHE STRING "")
set(CMAKE_SYSTEM_PROCESSOR aarch64 CACHE STRING "")
set(CMAKE_HOST_SYSTEM_NAME Linux CACHE STRING "")
set(CMAKE_HOST_SYSTEM_PROCESSOR x86_64 CACHE STRING "")

# Prefer the compiler bundled with this project.  Fall back to the historical
# /usr/aarch64-none-elf installation or a compiler already available in PATH.
get_filename_component(_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_BUNDLED_TOOLCHAIN
    "${_PROJECT_ROOT}/toolchains/arm-gnu-toolchain-15.3.rel1-x86_64-aarch64-none-elf")

if(EXISTS "${_BUNDLED_TOOLCHAIN}/bin/aarch64-none-elf-gcc")
    set(TOOLCHAIN_PATH "${_BUNDLED_TOOLCHAIN}" CACHE PATH "AArch64 bare-metal toolchain root" FORCE)
elseif(EXISTS "/usr/aarch64-none-elf/bin/aarch64-none-elf-gcc")
    set(TOOLCHAIN_PATH "/usr/aarch64-none-elf" CACHE PATH "AArch64 bare-metal toolchain root" FORCE)
else()
    find_program(_AARCH64_GCC aarch64-none-elf-gcc)
    find_program(_AARCH64_GXX aarch64-none-elf-g++)
    if(NOT _AARCH64_GCC OR NOT _AARCH64_GXX)
        message(FATAL_ERROR
            "Could not find the AArch64 bare-metal compiler. "
            "Expected the bundled toolchain at ${_BUNDLED_TOOLCHAIN}, "
            "/usr/aarch64-none-elf, or aarch64-none-elf-gcc/g++ in PATH.")
    endif()
    get_filename_component(_TOOLCHAIN_BIN "${_AARCH64_GCC}" DIRECTORY)
    get_filename_component(TOOLCHAIN_PATH "${_TOOLCHAIN_BIN}" DIRECTORY)
    set(TOOLCHAIN_PATH "${TOOLCHAIN_PATH}" CACHE PATH "AArch64 bare-metal toolchain root" FORCE)
endif()

set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/bin/aarch64-none-elf-gcc" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/bin/aarch64-none-elf-g++" CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PATH}/bin/aarch64-none-elf-gcc" CACHE FILEPATH "" FORCE)
set(CMAKE_OBJCOPY "${TOOLCHAIN_PATH}/bin/aarch64-none-elf-objcopy" CACHE FILEPATH "" FORCE)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE STRING "")

set(CMAKE_C_FLAGS "-march=armv8-a -mtune=cortex-a76 -Wall -Wextra -mstrict-align" CACHE STRING "C flags")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -fno-rtti" CACHE STRING "C++ flags")
