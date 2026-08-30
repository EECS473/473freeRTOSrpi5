#include "rp1_uart0.h"
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

void _exit(int /* status */)
{
    while (1)
    {
    }
}

int _write(int /* file */, const char* ptr, int len)
{
    for (int i = 0; i < len; ++i)
    {
        rp1Uart0Putc(ptr[i]);
    }
    return len;
}

int _read(int /* file */, char* /* ptr */, int /* len */)
{
    return 0;
}
int _close(int /* file */)
{
    return -1;
}
int _lseek(int /* file */, int /* ptr */, int /* dir */)
{
    return 0;
}

int _fstat(int /* file */, struct stat* st)
{
    st->st_mode = S_IFCHR;
    return 0;
}
int _isatty(int /* file */)
{
    return 1;
}

void* _sbrk(ptrdiff_t incr)
{
    extern char __heap_start;
    static char* heap_end;
    if (heap_end == (void*)0)
        heap_end = &__heap_start;
    char* prev_heap_end = heap_end;
    heap_end += incr;
    return (void*)prev_heap_end;
}

int _kill(int /* pid */, int /* sig */)
{
    return -1;
}

int _getpid(void)
{
    return 1;
}
