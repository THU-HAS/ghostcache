# Proof-of-concept for "GhostCache"

## Description

In RISC (ARM and RISC-V) processors, the L1 instruction cache (L1I\$) and data cache (L1D\$) can be implemented with weak coherence. Therefore, we can make the instruction data in L1I$ through function calls, and modify the instruction data through self-modifying code (smc) technology. The modified result is in L1D\$. Unless the instruction data in L1I\$ is evicted or the Invalidate instruction is used, the call result will still get the stale value in L1I\$. 
Based on the above principle, we can create an L1I\$ side channel attack primitive to observe whether each cache set of L1I\$ is evicted. We propose two attack primitives, `Modify+Recall` and `Call+ModifyCall`.

The attack scenario is based on the fact that the stale cache line within L1I\$ persists, even when the context switches between different processes or privilege levels. Thus, we can use the L1I\$ side channel to observe the cache set eviction status and then infer sensitive execution information. Three types of covert channels can be implemented based on the Modify+Recall in RISC-V processors:
- Intra-process Covert Channel
  - Explore to observe the L1I\$ side effect within the same process. The side effect can be caused by transient execution attacks such as Spectre. As Spectre needs a covert channel to persist the secret data gained in misprediction, the Modify+Recall can be a potential covert channel.
- Inter-process Covert Channel
  - Explore the secret-dependent cache set eviction status between different processes.
- Cross-privilege-boundary Covert Channel
  - Similar to above, but the attacker and victim are on different privilege levels.

Responsible Disclosure: We have disclosed our work and results to the affected vendors and received responses. ARM PSIRT appreciated our disclosure and accepted the risk of GhostCache on November 6, 2024. SiFive acknowledges the applicability of our attack scenario and plans to disclose it via a security bulletin.

## How to run the `Proof-of-Concept` (PoC)

```shell
cd pocs
make primitive run_primitive # detect whether the cache is weak coherence

make modify+recall run_modify+recall # using Modify+Recall to observe the cache set eviction status

make call+modifycall run_call+modifycall # using Call+ModifyCall to observe the cache set eviction status

make fullcache run_fullcache # using Modify+Recall to observe the cache set eviction status

make fullset_spectre_rsb run_fullset_spectre_rsb # proof-of-concept for Spectre-RSB gadget and unmask IC gadget in ARM Cortex-A76
```

Please note that the output may vary depending on the cache state and unavoidable noise. 

**Major Claims for reproducibility**: While the output may not be consistent, a **stable expected behavior** is described for each example (e.g., in worst case, at most 10 runs, we can get expected behavior in one of the runs).

**Port `primitive` to other chip**: The requirement is mainly the size, #way, and #set of L1I$, so the macros and defined structure parameters in smc.h need to be modified to adapt to different processors of `ARM` and `RISC-V`. For other `ISA`, the `smc.h` and the self-modify-code (smc) in `smc_primitive.c` need to be modified to adapt to the specific ISA.

## Example output for `primitive` in ARM Cortex-A76

**Stable expected behavior:**

When `without fence`, the initial return value of `func` is 1, and the modified return value is also 1 (more than half). This indicates that the cache line is not evicted, and the stale value is still in L1I\$.

When `with fence`, the initial return value of `func` is 1, but the modified return value is 2. This indicates that the cache line has been evicted, and the new value is fetched from memory.

When `evict func`, the initial return value of `func` is still 1, but the modified return value becomes 2, indicating that the cache line was evicted and updated correctly.

```shell
$ make primitive run_primitive
mkdir -p ././out
cc -static -O0 smc_primitive.c -o ./out/primitive
objdump -d --source-comment -Sr ./out/primitive > ./out/primitive.s
taskset -c 0 ./out/primitive
primitive
without fence
inital ret of `func`: 1 -> modify -> recall ret of `func`: 1
inital ret of `func`: 1 -> modify -> recall ret of `func`: 1
inital ret of `func`: 1 -> modify -> recall ret of `func`: 1
inital ret of `func`: 1 -> modify -> recall ret of `func`: 1
inital ret of `func`: 1 -> modify -> recall ret of `func`: 1
with fence
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
evict func
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
inital ret of `func`: 1 -> modify -> recall ret of `func`: 2
```

## Example output for `fullcache` in SiFive P550

**Stable expected behavior:**

We expect to see the `[ 42:0] 9 [ 42:1] 9 [ 42:2] 9 [ 42:3] 9` as one of the lines in the output, which indicates that the cache set 42 is evicted.

```shell
$ make fullcache run_fullcache
mkdir -p ././out
cc -static -O0 smc_fullcache.c -o ./out/fullcache
objdump -d --source-comment -Sr ./out/fullcache > ./out/fullcache.s
taskset -c 0 ./out/fullcache
max addr: 0x7fffb82d3000
addr: 7fffb82b3000, tag: 3fffdc159, idx: 40, offset: 0
Cache set: 42 to be observed
[ 23:0] 9 [ 23:1] 9 [ 23:2] 9 [ 23:3] 9 
[ 24:0] 9 [ 24:1] 9 [ 24:2] 9 [ 24:3] 9 
[ 25:0] 9 [ 25:1] 9 [ 25:2] 9 [ 25:3] 9 
[ 26:0] 9 [ 26:1] 9 [ 26:2] 9 [ 26:3] 9 
[ 27:3] 9 
[ 28:1] 9 [ 28:2] 9 
[ 29:0] 9 [ 29:2] 8 [ 29:3] 8 
[ 30:0] 9 [ 30:1] 9 [ 30:2] 8 [ 30:3] 9 
[ 31:0] 9 [ 31:1] 9 [ 31:2] 9 [ 31:3] 9 
[ 32:0] 9 [ 32:1] 9 [ 32:2] 9 [ 32:3] 9 
[ 33:0] 9 [ 33:1] 9 [ 33:2] 9 [ 33:3] 9 
[ 37:0] 9 
[ 42:0] 9 [ 42:1] 9 [ 42:2] 9 [ 42:3] 9 
[ 43:0] 9 [ 43:1] 9 [ 43:2] 9 [ 43:3] 9 
[ 44:0] 9 [ 44:1] 9 [ 44:2] 9 [ 44:3] 9 
[ 45:0] 9 [ 45:1] 9 [ 45:2] 9 [ 45:3] 9 
[ 61:0] 8 
[ 84:1] 8 [ 84:3] 8 
[ 88:0] 9 
[ 92:0] 9 
[116:1] 9 [116:2] 9 
[120:0] 9 
[124:0] 9
```

## Example output for `fullcache` in ARM Cortex-A76

**Stable expected behavior:**

```shell
$ make fullcache run_fullcache
mkdir -p ././out
cc -static -O0 smc_fullcache.c -o ./out/fullcache
objdump -d --source-comment -Sr ./out/fullcache > ./out/fullcache.s
taskset -c 0 ./out/fullcache
max addr: 0xffff99f2d000
addr: ffff99f0d000, tag: 3fffe67c3, idx: 40, offset: 0
Cache set: 42 to be observed
[ 17:0] 8 [ 17:2] 8 
[ 18:1] 8 [ 18:2] 9 
[ 19:1] 8 [ 19:2] 8 
[ 20:1] 9 
[ 22:1] 9 
[ 23:0] 9 [ 23:1] 8 [ 23:3] 9 
[ 25:1] 9 [ 25:3] 9 
[ 42:0] 9 [ 42:1] 9 [ 42:2] 9 [ 42:3] 8
```

You can modify the `smc_fullcache.c` (at line 297 `int cache_set = 42;`) to change the cache set to be observed. The expected output should show the eviction status of the specified cache set.

For example the below output is from `int cache_set = 88;` at `Cortex-A76`:

```shell
$ make fullcache run_fullcache
mkdir -p ././out
cc -static -O0 smc_fullcache.c -o ./out/fullcache
objdump -d --source-comment -Sr ./out/fullcache > ./out/fullcache.s
taskset -c 0 ./out/fullcache
max addr: 0xffff843c0000
addr: ffff843a0000, tag: 3fffe10e8, idx: 0, offset: 0
Cache set: 88 to be observed
[ 17:1] 9 [ 17:2] 8 
[ 18:1] 9 [ 18:2] 9 
[ 19:1] 9 [ 19:2] 9 
[ 20:0] 9 
[ 21:0] 9 [ 21:3] 8 
[ 22:0] 8 [ 22:1] 9 
[ 23:1] 9 
[ 24:0] 9 [ 24:3] 9 
[ 25:1] 9 [ 25:2] 8 [ 25:3] 9 
[ 88:0] 9 [ 88:1] 9 [ 88:2] 9 [ 88:3] 9 
[ 89:0] 8 [ 89:1] 8
```

## Website fingerprinting attack

Please see [./case_fingerprinting/README.md](./case_fingerprinting/README.md).

## Proof-of-concept Spectre and RSA leakage

Please see [./pocs/README.md](./pocs/README.md).

## Partial Tested Environment

### ARM Cortex-A76

```shell
$ uname -a
Linux **** 6.8.0-1013-raspi #14-Ubuntu SMP PREEMPT_DYNAMIC Wed Oct  2 15:14:53 UTC 2024 aarch64 aarch64 aarch64 GNU/Linux

$ lscpu
Architecture:             aarch64
  CPU op-mode(s):         32-bit, 64-bit
  Byte Order:             Little Endian
CPU(s):                   4
  On-line CPU(s) list:    0-3
Vendor ID:                ARM
  Model name:             Cortex-A76
    Model:                1
    Thread(s) per core:   1
    Core(s) per cluster:  4
    Socket(s):            -
    Cluster(s):           1
    Stepping:             r4p1
    CPU(s) scaling MHz:   67%
    CPU max MHz:          2400.0000
    CPU min MHz:          1500.0000
    BogoMIPS:             108.00
    Flags:                fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid a
                          simdrdm lrcpc dcpop asimddp
Vulnerabilities:          
  Gather data sampling:   Not affected
  Itlb multihit:          Not affected
  L1tf:                   Not affected
  Mds:                    Not affected
  Meltdown:               Not affected
  Mmio stale data:        Not affected
  Reg file data sampling: Not affected
  Retbleed:               Not affected
  Spec rstack overflow:   Not affected
  Spec store bypass:      Mitigation; Speculative Store Bypass disabled via prctl
  Spectre v1:             Mitigation; __user pointer sanitization
  Spectre v2:             Mitigation; CSV2, BHB
  Srbds:                  Not affected
  Tsx async abort:        Not affected
```

### SiFive P550

```shell
$ uname -a
Linux **** 6.6.18-eic7x #2024.09.10.18.37+ SMP Tue Sep 10 18:40:46 CST 2024 riscv64 GNU/Linux
$ lscpu
Architecture:           riscv64
  Byte Order:           Little Endian
CPU(s):                 4
  On-line CPU(s) list:  0-3
Vendor ID:              0x489
  Model name:           eswin,eic770x
    CPU family:         0x8000000000000008
    Model:              0x6220425
    Thread(s) per core: 1
    Core(s) per socket: 4
    Socket(s):          1
    CPU(s) scaling MHz: 100%
    CPU max MHz:        1400.0000
    CPU min MHz:        24.0000
```
