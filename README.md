# Backgroud
Porting an RTOS to a multicore processor is an essential part of an embedded software engineer’s knowledge system.
However, when choosing a multicore processor, ordinary consumers often find it difficult to obtain mainstream chips like
the TC397 either because the cost is high or because the purchasing channels are limited. Choosing multicore Cortex-M or
Cortex-R processors faces similar issues. To save time, I decided simply to use the Raspberry Pi 5, which features four
Cortex-A cores, allowing me to work directly on SMP RTOS development.

When I began porting FreeRTOS and looked into Raspberry Pi documentation, I accidentally discovered the Pico series -
a set of inexpensive multicore Cortex-M processors with abundant resources. For embedded developers familiar with Cortex-M,
this would be an excellent choice. But since I had already purchased the Raspberry Pi 5B, I could only continue working on
the ARMv8-A Cortex-A76 architecture.

After several months of exploring, I realized this wasn’t a bad thing at all. It gave me a brand-new understanding of A-class
processors, and I discovered that the underlying principles are remarkably similar to those of many MCUs. This deepened and
strengthened my understanding of embedded systems as a whole. Along the way, I had the chance to dive into device trees,
assembly, linker script, exception level, interrupt, etc. With limited Raspberry Pi 5 documentation, by referring to
Linux driver code and debugged step by step until I finally got FreeRTOS running.

# Install compilers
Download ARM GCC cross-compiler [AArch64 bare-metal target (aarch64-none-elf)] from
https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads on Ubuntu and unzip it to /usr and rename the folder name to
aarch64-none-elf.


# Compile
Change current working directory to pi5_freertos.

CMake config and generation.
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=aarch64-none-elf.toolchain.cmake
```
Build.
```bash
cmake --build build -j16
```


# Running
Replace the kernel8.elf file in SD card and power on.
