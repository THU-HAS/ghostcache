#define _GNU_SOURCE

#include <assert.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "smc.h"

__attribute__((aligned(0x1000))) void func_evict()
{
    asm volatile(".rept 1024*16\n\t"
                 "nop\n\t"
                 ".endr\n\t");
}

void fence()
{
#if defined(__x86_64__)
    __asm__ __volatile__("mfence" ::: "memory");
#elif defined(__aarch64__)
    __asm__ __volatile__("dsb sy" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

inline static size_t flush(void *p)
{
#if defined(__x86_64__)
    asm volatile("clflush (%0)\n" ::"c"((uint64_t)p) : "rax");
#elif defined(__aarch64__)
    asm volatile("DC CIVAC, %0\n" ::"r"((uint64_t)p) : "memory");
    asm volatile("ic ivau, %0\n" ::"r"((uint64_t)p) : "memory");
#else
    perror("Not implement");
#endif
}

int main()
{
    long page_size = sysconf(_SC_PAGESIZE);

    void *mem = mmap(NULL, page_size * 16, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }
    unsigned char code[] = {
#if defined(__x86_64__)
        // x86, https://defuse.ca/online-x86-assembler.htm#disassembly
        0xB8, 0x00, 0x00, 0x00, 0x00, //  mov eax, 0
        0x83, 0xC0, 0x01,             //  add eax, 2
        0xC3,                         //  ret
#elif defined(__aarch64__)
        // arm, https://shell-storm.org/online/Online-Assembler-and-Disassembler
        0x00, 0x00, 0x80, 0xd2, // mov x0, #0
        0x00, 0x04, 0x00, 0x91, // add x0, x0, #2
        0xc0, 0x03, 0x5f, 0xd6, // ret
#elif defined(__riscv)
        // rv, https://rvcodec-js.vercel.app/
        0x37, 0x5, 0x00, 0x00,  // lui a0, 0  <=> lui x10, 0
        0x13, 0x05, 0x15, 0x00, // addi a0, a0, 1 <=> addi x10, x10, 1
        0x67, 0x80, 0x00, 0x00, // jalr zero, 0(ra))  <=> jalr x0, 0(x1)
#else
#endif
    };
    memset(mem, 0, page_size);
    memcpy(mem, code, sizeof(code));

    if (mprotect(mem, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
    {
        perror("mprotect");
        munmap(mem, page_size);
        return 1;
    }
    int (*func)() = mem;
    volatile int result[5][2];

    printf("primitive\n");

    printf("without fence\n");
    for (int i = 0; i < 5; i++)
    {
#if defined(__x86_64__)
        flush(func);
#elif defined(__aarch64__)
        flush(func);
#elif defined(__riscv)
        asm volatile("fence.i" ::: "memory");
#endif
        fence();

#if defined(__x86_64__)
        ((uint8_t *)func)[7] = 0x1;
#elif defined(__aarch64__)
        ((uint8_t *)func)[5] = 0x4;
#elif defined(__riscv)
        ((uint8_t *)func)[6] = 0x15;
#endif
        for (int j = 0; j < 1000; j++)
        {
            asm volatile("nop" ::: "memory");
        }
        result[i][0] = func();
#if defined(__x86_64__) //
        ((uint8_t *)func)[7] = 0x2;
#elif defined(__aarch64__) //
        ((uint8_t *)func)[5] = 0x8;
#elif defined(__riscv)     //
        ((uint8_t *)func)[6] = 0x25;
#endif
        for (int j = 0; j < 1000; j++)
        {
            asm volatile("nop" ::: "memory");
        }
        result[i][1] = func();
    }
    for (int i = 0; i < 5; i++)
    {
        printf("inital ret of `func`: %d -> modify -> ", result[i][0]);
        printf("recall ret of `func`: %d\n", result[i][1]);
    }

    printf("with fence\n");
    for (int i = 0; i < 5; i++)
    {
#if defined(__x86_64__)
        flush(func);
#elif defined(__aarch64__)
        flush(func);
#elif defined(__riscv)
        asm volatile("fence.i" ::: "memory");
#endif

#if defined(__x86_64__)
        ((uint8_t *)func)[7] = 0x1;
#elif defined(__aarch64__)
        ((uint8_t *)func)[5] = 0x4;
#elif defined(__riscv)
        ((uint8_t *)func)[6] = 0x15;
#endif
        FENCE_ICACHE
        result[i][0] = func();
#if defined(__x86_64__) //
        ((uint8_t *)func)[7] = 0x2;
#elif defined(__aarch64__) //
        ((uint8_t *)func)[5] = 0x8;
#elif defined(__riscv)     //
        ((uint8_t *)func)[6] = 0x25;
#endif
        FENCE_ICACHE
        result[i][1] = func();
    }
    for (int i = 0; i < 5; i++)
    {
        printf("inital ret of `func`: %d -> modify -> ", result[i][0]);
        printf("recall ret of `func`: %d\n", result[i][1]);
    }

    printf("evict func\n");
    for (int i = 0; i < 5; i++)
    {
#if defined(__x86_64__)
        flush(func);
#elif defined(__aarch64__)
        flush(func);
#elif defined(__riscv)
        asm volatile("fence.i" ::: "memory");
#endif

#if defined(__x86_64__)
        ((uint8_t *)func)[7] = 0x1;
#elif defined(__aarch64__)
        ((uint8_t *)func)[5] = 0x4;
#elif defined(__riscv)
        ((uint8_t *)func)[6] = 0x15;
#endif
        func_evict();
        result[i][0] = func();
#if defined(__x86_64__) //
        ((uint8_t *)func)[7] = 0x2;
#elif defined(__aarch64__) //
        ((uint8_t *)func)[5] = 0x8;
#elif defined(__riscv)     //
        ((uint8_t *)func)[6] = 0x25;
#endif
        func_evict();
        result[i][1] = func();
    }
    for (int i = 0; i < 5; i++)
    {
        printf("inital ret of `func`: %d -> modify -> ", result[i][0]);
        printf("recall ret of `func`: %d\n", result[i][1]);
    }

    munmap(mem, page_size);

    return 0;
}