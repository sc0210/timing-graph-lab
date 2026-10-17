# Timing Graph Lab — results summary

- rows: 4598; commits: 43a62d58b247
- CPU: Intel(R) Xeon(R) Processor @ 2.80GHz; build: Release
- seeds: 20261017, 20261018, 20261019; times are medians over repetitions (µs), then median over seeds; `spread` = (max-min)/median of per-seed medians

## Gate D — correctness

All rows passed verification against the reference engine.

Results not bit-identical to the reference (but within tolerance): none

## Experiment A — Adjacency List vs CSR

_(no data)_

CSR break-even: number of full arrival+required passes after which CSR's (build) cost is repaid:

_(no data)_

## Experiment B — topology (CSR, serial pull)

| topology | nodes | edges | sources | sinks | max fo | max fi | depth | median level width | arrival µs | edges/s | CV |
|---|---|---|---|---|---|---|---|---|---|---|---|
| chain | 100000 | 99999 | 1 | 1 | 1 | 1 | 100000 | 1 | 1,782.1 | 5.611e+07 | 28.4% |
| layered | 100000 | 396000 | 1000 | 2866 | 14 | 4 | 100 | 1000 | 5,079.3 | 7.796e+07 | 26.4% |
| layered | 1000000 | 3960000 | 10000 | 28140 | 18 | 4 | 100 | 10000 | 123,276.2 | 3.212e+07 | 24.1% |
| random | 100000 | 400000 | 1799 | 20055 | 46 | 16 | 82 | 335 | 4,963.3 | 8.059e+07 | 58.2% |
| random | 1000000 | 4000000 | 18228 | 199838 | 57 | 16 | 106 | 1446 | 151,214.7 | 2.645e+07 | 16.2% |
| reconvergent | 100000 | 396000 | 1000 | 2981 | 17 | 4 | 100 | 1000 | 7,194.2 | 5.504e+07 | 32.2% |
| reconvergent | 1000000 | 3960000 | 10000 | 29983 | 19 | 4 | 100 | 10000 | 136,128.1 | 2.909e+07 | 17.6% |

## Experiment C — incremental vs full recompute (CSR)

S_inc = T_full / T_incremental (> 1: incremental faster). affected = nodes in the fanout cone (AT) ∪ fanin cone (RT) of the edited edges; changed = nodes whose AT or RT changed.

_(no data)_

## Experiment D — level-parallel propagation (CSR)

Speedup(p) = T_1 / T_p with T_1 = serial pull AT+RT; efficiency = speedup / p. Algorithm-only = arrival+required; end-to-end adds levelize + pool_start.

| topology | nodes | depth | median width | barrier | threads | AT+RT µs | speedup | efficiency | end-to-end µs | e2e speedup | µs/level |
|---|---|---|---|---|---|---|---|---|---|---|---|
| chain | 100000 | 100000 | 1 | block | 1 | 13,228.8 | 0.23 | 23% | 16,011.5 | 0.19 | 0.13 |
| chain | 100000 | 100000 | 1 | block | 2 | 417,636.6 | 0.01 | 0% | 420,501.7 | 0.01 | 4.18 |
| chain | 100000 | 100000 | 1 | block | 4 | 9,360,590.4 | 0.00 | 0% | 9,363,506.5 | 0.00 | 93.61 |
| chain | 100000 | 100000 | 1 | spin | 1 | 12,527.9 | 0.24 | 24% | 15,310.4 | 0.20 | 0.13 |
| chain | 100000 | 100000 | 1 | spin | 2 | 70,601.8 | 0.04 | 2% | 73,466.5 | 0.04 | 0.71 |
| chain | 100000 | 100000 | 1 | spin | 4 | 192,674.2 | 0.02 | 0% | 195,620.2 | 0.02 | 1.93 |
| layered | 100000 | 100 | 1000 | block | 1 | 9,746.3 | 0.96 | 96% | 17,173.6 | 0.54 | 97.46 |
| layered | 100000 | 100 | 1000 | block | 2 | 14,578.8 | 0.64 | 32% | 22,110.8 | 0.42 | 145.79 |
| layered | 100000 | 100 | 1000 | block | 4 | 17,218.8 | 0.54 | 14% | 24,823.3 | 0.38 | 172.19 |
| layered | 100000 | 100 | 1000 | spin | 1 | 9,998.8 | 0.93 | 93% | 17,425.9 | 0.53 | 99.99 |
| layered | 100000 | 100 | 1000 | spin | 2 | 7,938.0 | 1.17 | 59% | 15,468.1 | 0.60 | 79.38 |
| layered | 100000 | 100 | 1000 | spin | 4 | 7,864.6 | 1.18 | 30% | 15,457.6 | 0.60 | 78.65 |
| layered | 1000000 | 100 | 10000 | block | 1 | 282,319.0 | 0.93 | 93% | 479,899.1 | 0.55 | 2823.19 |
| layered | 1000000 | 100 | 10000 | block | 2 | 182,167.2 | 1.44 | 72% | 379,863.1 | 0.69 | 1821.67 |
| layered | 1000000 | 100 | 10000 | block | 4 | 130,461.9 | 2.01 | 50% | 328,228.4 | 0.80 | 1304.62 |
| layered | 1000000 | 100 | 10000 | spin | 1 | 257,991.6 | 1.02 | 102% | 455,571.7 | 0.58 | 2579.92 |
| layered | 1000000 | 100 | 10000 | spin | 2 | 177,257.9 | 1.48 | 74% | 374,940.0 | 0.70 | 1772.58 |
| layered | 1000000 | 100 | 10000 | spin | 4 | 106,379.4 | 2.47 | 62% | 304,135.7 | 0.86 | 1063.79 |
| random | 100000 | 82 | 335 | block | 1 | 9,859.1 | 0.91 | 91% | 15,156.8 | 0.59 | 120.23 |
| random | 100000 | 82 | 335 | block | 2 | 10,766.2 | 0.83 | 42% | 16,160.3 | 0.55 | 131.29 |
| random | 100000 | 82 | 335 | block | 4 | 13,960.0 | 0.64 | 16% | 19,405.9 | 0.46 | 170.24 |
| random | 100000 | 82 | 335 | spin | 1 | 8,948.4 | 1.00 | 100% | 14,246.1 | 0.63 | 109.13 |
| random | 100000 | 82 | 335 | spin | 2 | 7,811.8 | 1.15 | 57% | 13,207.1 | 0.68 | 95.27 |
| random | 100000 | 82 | 335 | spin | 4 | 5,788.9 | 1.55 | 39% | 11,253.8 | 0.80 | 70.60 |
| random | 1000000 | 106 | 1446 | block | 1 | 305,200.2 | 0.99 | 99% | 490,957.2 | 0.62 | 2879.25 |
| random | 1000000 | 106 | 1446 | block | 2 | 198,576.7 | 1.53 | 76% | 384,435.3 | 0.79 | 1873.37 |
| random | 1000000 | 106 | 1446 | block | 4 | 137,481.4 | 2.21 | 55% | 323,417.8 | 0.94 | 1296.99 |
| random | 1000000 | 106 | 1446 | spin | 1 | 310,585.9 | 0.98 | 98% | 496,343.0 | 0.61 | 2930.06 |
| random | 1000000 | 106 | 1446 | spin | 2 | 198,130.8 | 1.53 | 77% | 383,987.6 | 0.79 | 1869.16 |
| random | 1000000 | 106 | 1446 | spin | 4 | 121,938.4 | 2.49 | 62% | 307,882.4 | 0.98 | 1150.36 |
| reconvergent | 100000 | 100 | 1000 | block | 1 | 12,431.8 | 1.12 | 112% | 23,002.2 | 0.61 | 124.32 |
| reconvergent | 100000 | 100 | 1000 | block | 2 | 11,631.5 | 1.20 | 60% | 22,314.6 | 0.63 | 116.32 |
| reconvergent | 100000 | 100 | 1000 | block | 4 | 17,092.3 | 0.82 | 20% | 27,814.9 | 0.50 | 170.92 |
| reconvergent | 100000 | 100 | 1000 | spin | 1 | 16,729.3 | 0.83 | 83% | 27,299.6 | 0.51 | 167.29 |
| reconvergent | 100000 | 100 | 1000 | spin | 2 | 9,411.3 | 1.48 | 74% | 20,085.1 | 0.69 | 94.11 |
| reconvergent | 100000 | 100 | 1000 | spin | 4 | 6,483.8 | 2.15 | 54% | 17,232.1 | 0.81 | 64.84 |
| reconvergent | 1000000 | 100 | 10000 | block | 1 | 288,563.5 | 1.04 | 104% | 503,444.7 | 0.59 | 2885.64 |
| reconvergent | 1000000 | 100 | 10000 | block | 2 | 200,250.3 | 1.50 | 75% | 415,241.9 | 0.72 | 2002.50 |
| reconvergent | 1000000 | 100 | 10000 | block | 4 | 130,043.2 | 2.30 | 58% | 345,114.5 | 0.87 | 1300.43 |
| reconvergent | 1000000 | 100 | 10000 | spin | 1 | 322,120.5 | 0.93 | 93% | 537,001.8 | 0.56 | 3221.20 |
| reconvergent | 1000000 | 100 | 10000 | spin | 2 | 177,879.0 | 1.68 | 84% | 392,860.1 | 0.76 | 1778.79 |
| reconvergent | 1000000 | 100 | 10000 | spin | 4 | 117,156.2 | 2.56 | 64% | 332,206.8 | 0.90 | 1171.56 |

## Experiment E — traversal (CSR)

Traversal cost only; BFS does not produce a timing order in general (column 'BFS order valid'). The last two columns run the same pull propagation over Kahn vs DFS orders.

_(no data)_

## Memory and noise

peak_rss_bytes is process-wide (the whole run, incl. reference engine and both representations); structure memory is the computed size of the graph arrays.

| topology | nodes | peak RSS MiB |
|---|---|---|
| chain | 100000 | 17.7 |
| layered | 100000 | 33.9 |
| layered | 1000000 | 299.7 |
| random | 100000 | 34.0 |
| random | 1000000 | 305.4 |
| reconvergent | 100000 | 33.9 |
| reconvergent | 1000000 | 303.1 |

141 configuration(s) with CV > 10% (median > 100 µs): layered/100000/csr/build/counting_sort/t1; layered/100000/csr/arrival/pull/t1; layered/100000/csr/arrival/push/t1; layered/100000/csr/required/pull/t1; layered/100000/csr/slack/rt_minus_at/t1; layered/100000/csr/levelize/longest_hop/t1; layered/100000/csr/arrival/level_parallel_spin/t1; layered/100000/csr/required/level_parallel_spin/t1; layered/100000/csr/pool_start/level_parallel_spin/t2; layered/100000/csr/arrival/level_parallel_spin/t2; layered/100000/csr/required/level_parallel_spin/t2; layered/100000/csr/pool_start/level_parallel_spin/t4; layered/100000/csr/arrival/level_parallel_spin/t4; layered/100000/csr/required/level_parallel_spin/t4; layered/100000/csr/arrival/level_parallel_block/t1

