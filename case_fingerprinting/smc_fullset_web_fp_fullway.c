#define _GNU_SOURCE

#include <assert.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include "smc.h"

#define L1I_ALIGN 0x4000
#define WAYLAND_DISPLAY_NAME "wayland-0"
#define XDG_RUNTIME_DIR_NAME "/run/user/1002" // replace with current UID

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

#define NOP_BLOCKS1         \
    asm volatile(           \
        ".rept 1024*16\n\t" \
        "nop\n\t"           \
        ".endr\n\t");

__attribute__((aligned(L1I_ALIGN))) void func_evict0()
{
    // NOP_BLOCKS1
    NOP_BLOCKS0
}

__attribute__((aligned(L1I_ALIGN))) void func_evict1()
{
    // NOP_BLOCKS1
    NOP_BLOCKS0
}

__attribute__((aligned(L1I_ALIGN))) void func_evict2()
{
    // NOP_BLOCKS1
    NOP_BLOCKS0
}

__attribute__((aligned(L1I_ALIGN))) void func_evict3()
{
    // NOP_BLOCKS1
    NOP_BLOCKS0
}

FILE* f;

void test(void *mem, long page_size, long evict_set)
{
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

    if (mprotect(mem, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
    {
        perror("mprotect");
        munmap(mem, page_size);
        return;
    }
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

    int count[L1I_SET_NUM][L1I_SET_SIZE], sum = 9;
    memset(count, 0, sizeof(count));
    for (int j = 0; j < sum; j++)
    {
        for (int k = 0; k < L1I_SET_NUM; k++)
        {
            for (size_t l = 0; l < L1I_SET_SIZE; l++)
            {
                func = func_list[k][l];
#if defined(__x86_64__)
                ((uint8_t *)func)[7] = 0x1;
#elif defined(__aarch64__)
                ((uint8_t *)func)[5] = 0x4;
                asm volatile("ic ivau, %0;isb sy;isb sy;" ::"r"((uint64_t)func) : "memory");
                asm volatile("dsb st;dsb sy;isb;isb sy;isb sy;");
#elif defined(__riscv)
                ((uint8_t *)func)[6] = 0x15;
#endif
            }
        }
#if defined(__riscv)
        asm volatile("fence.i");
#endif
        for (int k = 0; k < L1I_SET_NUM; k++)
        {
            for (size_t l = 0; l < L1I_SET_SIZE; l++)
            {        
                func = func_list[k][l];
                result = func();
#if defined(__x86_64__) //
                ((uint8_t *)func)[7] = 0x2;
#elif defined(__aarch64__) //
                ((uint8_t *)func)[5] = 0x8;
#elif defined(__riscv)     //
                ((uint8_t *)func)[6] = 0x25;
#endif
            }
        }
#if defined(__aarch64__)
        asm volatile("dsb st;dsb sy;isb;isb sy;isb sy;");
#endif

        // syscall(170);
        sched_yield();
#if defined(__aarch64__)
        for (volatile register int l = 0; l < 100000; l++)
        {
            asm volatile("nop\n\t");
        }
#elif defined(__riscv)
        for (volatile register int i = 0; i < 10; i++)
        {
            for (volatile register int l = 0; l < 10000; l++)
            {
                asm volatile("nop\n\t");
            }
            sched_yield();
        }
#endif
        sched_yield();

        for (int k = 0; k < L1I_SET_NUM; k++)
        {
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
    va.address = (uint64_t)func;
    assert((uint64_t)func == (uint64_t)va.address);
    for (int j = 0; j < L1I_SET_NUM; j++)
    {
        int has_output = 0;
        int set_count = 0;
        for (int l = 0; l < L1I_SET_SIZE; l++)
        {
            if (count[j][l] > sum - 2)
            {
                // printf("[%3d:%1d] %1d ", j, l, count[j][l]);
                has_output = 1;
                set_count += 1;
            }
        }
        fprintf(f, "%3d ", set_count);
    }
    fprintf(f, "\n");
    return;
}

void* open_ws(void* ws) {
    usleep(100000);
    if (setenv("WAYLAND_DISPLAY", WAYLAND_DISPLAY_NAME, 1) != 0) {
        perror("Failed to set WAYLAND_DISPLAY environment variable");
        pthread_exit(NULL);
    }

    if (setenv("XDG_RUNTIME_DIR", XDG_RUNTIME_DIR_NAME, 1) != 0) {
        perror("Failed to set XDG_RUNTIME_DIR environment variable");
        pthread_exit(NULL);
    }
    char cmd[256];
    // set proxy if necessary
    sprintf(cmd, "taskset -c 0 chromium --proxy-server=http://localhost:10809 --headless --disable-gpu --incognito --screenshot=test.png %s", (char*)ws);
    printf("cmd: %s\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Failed to launch browser with command: %s\n", cmd);
    } else {
        printf("Browser launched successfully on DISPLAY=:0\n");
    }
    printf("pthread exit\n");
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <url> <tag>\n", argv[0]);
        return 1;
    }
    char filename[128];
    sprintf(filename, "data/result_%s.txt", argv[2]);
    printf("filename: %s\n", filename);
    f=fopen(filename, "w");
    long page_size = sysconf(_SC_PAGESIZE);
    void *mem = mmap(NULL, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }
    printf("max addr: %p\n", mem + page_size * 32);

    VirtualAddress va;
    va.address = (uint64_t)mem;
    printf("addr: %lx, tag: %lx, idx: %x, offset: %x\n",
           va.address, (uint64_t)va.split.tag, va.split.index, va.split.offset);

    pthread_t p;
    pthread_create(&p, NULL, open_ws, argv[1]);

    for (int i = 0; i < 1000; i++)
    {
        test(mem, page_size, 0);

#if defined(__aarch64__)
        for (volatile register int l = 0; l < 5000000; l++)
        {
            asm volatile("nop\n\t");
        }
#endif
    }
    munmap(mem, page_size * 32);
    system("pkill -f chromium");
    printf("main exit\n");
    return 0;
}