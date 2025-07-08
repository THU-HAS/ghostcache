#ifndef __SMC_H__
#define __SMC_H__
#include <stdint.h>

#if defined(__x86_64__)
// AMD EPYC 9554: 32kB icache, 8way, 64B cache block
typedef struct
{
    uint64_t offset : 6; // line offset
    uint64_t index : 6;  // set index
    uint64_t tag : 52;   // tag
} VirtualAddressSplit;
#elif defined(__aarch64__)
// Cortex A76: 64kB icache, 4way, 64B cache block
typedef struct
{
    uint64_t offset : 6; // line offset
    uint64_t index : 8;  // set index
    uint64_t tag : 50;   // tag
} VirtualAddressSplit;
#define L1I_SET_NUM 256
// a.k.a. associativity or way size
#define L1I_SET_SIZE 4
#elif defined(__riscv)
// #define __riscv_c910 1
#define __riscv_p550 1
#if defined(__riscv_c910)
// C910: 64kB icache, 2way, 64B cache block
typedef struct
{
    uint64_t offset : 6; // line offset
    uint64_t index : 9;  // set index
    uint64_t tag : 49;   // tag
} VirtualAddressSplit;
#define L1I_SET_NUM 256
#define L1I_SET_SIZE 4
#elif defined(__riscv_p550)
// P550: 32kB icache, 4way, 64B cache block
typedef struct
{
    uint64_t offset : 6; // line offset
    uint64_t index : 7;  // set index
    uint64_t tag : 51;   // tag
} VirtualAddressSplit;
#define L1I_SET_NUM 128
#define L1I_SET_SIZE 4
#endif
#else
#error("unsupport architecture!")
#endif

#if defined(__x86_64__) //
#define FENCE_ICACHE asm volatile("mfence;lfence;sfence;cpuid;");
#elif defined(__aarch64__) //
#define FENCE_ICACHE asm volatile("ic ivau, x0;dsb ish;isb sy");
#elif defined(__riscv) //
#define FENCE_ICACHE asm volatile("fence.i");
#else //
#endif

#if defined(__x86_64__)    //
#elif defined(__aarch64__) //
#elif defined(__riscv)     //
#endif

typedef union
{
    uint64_t address;
    VirtualAddressSplit split;
} VirtualAddress;

#endif
