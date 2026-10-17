# Timing Graph Lab — results summary

- rows: 7992; commits: 43a62d58b247
- CPU: Intel(R) Xeon(R) Processor @ 2.80GHz; build: Release
- seeds: 20261017, 20261018, 20261019; times are medians over repetitions (µs), then median over seeds; `spread` = (max-min)/median of per-seed medians

## Gate D — correctness

All rows passed verification against the reference engine.

Results not bit-identical to the reference (but within tolerance): none

## Experiment A — Adjacency List vs CSR

| topology | nodes | edges | phase | list µs | csr µs | list/csr | seed spread |
|---|---|---|---|---|---|---|---|
| layered | 10000 | 39600 | build/counting_sort | 4,913.9 | 1,269.3 | 3.87 | 55% |
| layered | 10000 | 39600 | topo_sort/kahn | 1,037.7 | 303.5 | 3.42 | 41% |
| layered | 10000 | 39600 | arrival/pull | 204.8 | 72.68 | 2.82 | 115% |
| layered | 10000 | 39600 | arrival/push | 353.5 | 216.9 | 1.63 | 75% |
| layered | 10000 | 39600 | required/pull | 189.1 | 175.0 | 1.08 | 61% |
| layered | 10000 | 39600 | edge_insert/append|rebuild | 72.77 | 1,888.5 | 0.04 | 63% |
| layered | 10000 | 39600 | structure MiB | 1.8 | 1.1 | 1.72 |  |
| layered | 100000 | 396000 | build/counting_sort | 169,147.0 | 91,968.3 | 1.84 | 8% |
| layered | 100000 | 396000 | topo_sort/kahn | 42,737.7 | 6,238.4 | 6.85 | 7% |
| layered | 100000 | 396000 | arrival/pull | 7,627.2 | 3,683.1 | 2.07 | 23% |
| layered | 100000 | 396000 | arrival/push | 31,065.5 | 7,993.2 | 3.89 | 18% |
| layered | 100000 | 396000 | required/pull | 9,108.2 | 3,927.6 | 2.32 | 8% |
| layered | 100000 | 396000 | edge_insert/append|rebuild | 2,820.4 | 102,460.7 | 0.03 | 11% |
| layered | 100000 | 396000 | structure MiB | 18.3 | 10.6 | 1.72 |  |
| layered | 500000 | 1980000 | build/counting_sort | 1,332,679.1 | 1,076,011.8 | 1.24 | 11% |
| layered | 500000 | 1980000 | topo_sort/kahn | 395,784.0 | 159,564.0 | 2.48 | 10% |
| layered | 500000 | 1980000 | arrival/pull | 65,685.7 | 51,026.1 | 1.29 | 29% |
| layered | 500000 | 1980000 | arrival/push | 280,895.3 | 203,418.9 | 1.38 | 7% |
| layered | 500000 | 1980000 | required/pull | 66,245.9 | 57,016.1 | 1.16 | 18% |
| layered | 500000 | 1980000 | edge_insert/append|rebuild | 21,038.9 | 1,074,589.6 | 0.02 | 9% |
| layered | 500000 | 1980000 | structure MiB | 91.4 | 52.9 | 1.73 |  |
| layered | 1000000 | 3960000 | build/counting_sort | 2,921,896.1 | 2,405,679.9 | 1.21 | 6% |
| layered | 1000000 | 3960000 | topo_sort/kahn | 929,786.5 | 381,333.6 | 2.44 | 4% |
| layered | 1000000 | 3960000 | arrival/pull | 141,232.4 | 133,077.5 | 1.06 | 27% |
| layered | 1000000 | 3960000 | arrival/push | 729,010.9 | 481,923.4 | 1.51 | 11% |
| layered | 1000000 | 3960000 | required/pull | 161,554.1 | 139,148.7 | 1.16 | 24% |
| layered | 1000000 | 3960000 | edge_insert/append|rebuild | 58,999.9 | 2,500,011.1 | 0.02 | 17% |
| layered | 1000000 | 3960000 | structure MiB | 182.8 | 105.9 | 1.73 |  |
| random | 10000 | 40000 | build/counting_sort | 3,495.8 | 1,416.1 | 2.47 | 114% |
| random | 10000 | 40000 | topo_sort/kahn | 629.1 | 365.3 | 1.72 | 96% |
| random | 10000 | 40000 | arrival/pull | 218.3 | 181.1 | 1.21 | 115% |
| random | 10000 | 40000 | arrival/push | 280.5 | 262.2 | 1.07 | 98% |
| random | 10000 | 40000 | required/pull | 191.1 | 158.9 | 1.20 | 93% |
| random | 10000 | 40000 | edge_insert/append|rebuild | 33.90 | 2,279.4 | 0.01 | 143% |
| random | 10000 | 40000 | structure MiB | 2.0 | 1.1 | 1.90 |  |
| random | 100000 | 400000 | build/counting_sort | 169,150.0 | 96,185.7 | 1.76 | 16% |
| random | 100000 | 400000 | topo_sort/kahn | 20,180.1 | 7,720.3 | 2.61 | 26% |
| random | 100000 | 400000 | arrival/pull | 9,362.4 | 4,061.5 | 2.31 | 37% |
| random | 100000 | 400000 | arrival/push | 24,322.9 | 8,154.2 | 2.98 | 17% |
| random | 100000 | 400000 | required/pull | 9,688.0 | 4,315.2 | 2.25 | 34% |
| random | 100000 | 400000 | edge_insert/append|rebuild | 1,911.4 | 106,025.9 | 0.02 | 30% |
| random | 100000 | 400000 | structure MiB | 20.3 | 10.7 | 1.90 |  |
| random | 500000 | 2000000 | build/counting_sort | 1,345,111.2 | 1,083,960.8 | 1.24 | 6% |
| random | 500000 | 2000000 | topo_sort/kahn | 214,035.6 | 137,341.0 | 1.56 | 10% |
| random | 500000 | 2000000 | arrival/pull | 75,157.6 | 70,446.3 | 1.07 | 17% |
| random | 500000 | 2000000 | arrival/push | 263,615.1 | 199,173.1 | 1.32 | 7% |
| random | 500000 | 2000000 | required/pull | 77,822.9 | 73,135.1 | 1.06 | 20% |
| random | 500000 | 2000000 | edge_insert/append|rebuild | 15,835.3 | 1,055,954.3 | 0.01 | 11% |
| random | 500000 | 2000000 | structure MiB | 101.5 | 53.4 | 1.90 |  |
| random | 1000000 | 4000000 | build/counting_sort | 3,122,579.3 | 2,473,195.7 | 1.26 | 7% |
| random | 1000000 | 4000000 | topo_sort/kahn | 539,955.6 | 360,415.9 | 1.50 | 16% |
| random | 1000000 | 4000000 | arrival/pull | 179,322.0 | 150,780.2 | 1.19 | 24% |
| random | 1000000 | 4000000 | arrival/push | 618,163.1 | 461,218.8 | 1.34 | 13% |
| random | 1000000 | 4000000 | required/pull | 197,725.5 | 167,779.7 | 1.18 | 21% |
| random | 1000000 | 4000000 | edge_insert/append|rebuild | 36,058.2 | 2,393,346.8 | 0.02 | 8% |
| random | 1000000 | 4000000 | structure MiB | 202.9 | 106.8 | 1.90 |  |
| reconvergent | 10000 | 39600 | build/counting_sort | 3,897.7 | 1,643.9 | 2.37 | 72% |
| reconvergent | 10000 | 39600 | topo_sort/kahn | 1,037.8 | 437.0 | 2.37 | 39% |
| reconvergent | 10000 | 39600 | arrival/pull | 265.6 | 119.3 | 2.23 | 79% |
| reconvergent | 10000 | 39600 | arrival/push | 338.1 | 295.1 | 1.15 | 52% |
| reconvergent | 10000 | 39600 | required/pull | 222.2 | 202.0 | 1.10 | 27% |
| reconvergent | 10000 | 39600 | edge_insert/append|rebuild | 94.53 | 2,367.4 | 0.04 | 45% |
| reconvergent | 10000 | 39600 | structure MiB | 1.8 | 1.1 | 1.73 |  |
| reconvergent | 100000 | 396000 | build/counting_sort | 194,262.1 | 100,511.4 | 1.93 | 12% |
| reconvergent | 100000 | 396000 | topo_sort/kahn | 48,622.1 | 7,313.5 | 6.65 | 16% |
| reconvergent | 100000 | 396000 | arrival/pull | 9,369.9 | 4,282.7 | 2.19 | 30% |
| reconvergent | 100000 | 396000 | arrival/push | 37,082.7 | 8,144.8 | 4.55 | 14% |
| reconvergent | 100000 | 396000 | required/pull | 10,218.3 | 4,070.5 | 2.51 | 7% |
| reconvergent | 100000 | 396000 | edge_insert/append|rebuild | 3,103.3 | 102,241.2 | 0.03 | 11% |
| reconvergent | 100000 | 396000 | structure MiB | 18.3 | 10.6 | 1.73 |  |
| reconvergent | 500000 | 1980000 | build/counting_sort | 1,258,159.5 | 1,081,392.7 | 1.16 | 10% |
| reconvergent | 500000 | 1980000 | topo_sort/kahn | 377,699.0 | 146,945.5 | 2.57 | 16% |
| reconvergent | 500000 | 1980000 | arrival/pull | 69,320.0 | 56,801.6 | 1.22 | 21% |
| reconvergent | 500000 | 1980000 | arrival/push | 293,072.6 | 227,989.3 | 1.29 | 8% |
| reconvergent | 500000 | 1980000 | required/pull | 66,617.0 | 62,850.8 | 1.06 | 17% |
| reconvergent | 500000 | 1980000 | edge_insert/append|rebuild | 18,850.1 | 1,039,643.9 | 0.02 | 10% |
| reconvergent | 500000 | 1980000 | structure MiB | 91.4 | 52.9 | 1.73 |  |
| reconvergent | 1000000 | 3960000 | build/counting_sort | 3,031,089.3 | 2,432,170.5 | 1.25 | 5% |
| reconvergent | 1000000 | 3960000 | topo_sort/kahn | 1,003,515.1 | 384,198.2 | 2.61 | 6% |
| reconvergent | 1000000 | 3960000 | arrival/pull | 177,668.6 | 143,519.9 | 1.24 | 30% |
| reconvergent | 1000000 | 3960000 | arrival/push | 798,469.1 | 535,923.1 | 1.49 | 6% |
| reconvergent | 1000000 | 3960000 | required/pull | 183,872.4 | 153,053.3 | 1.20 | 46% |
| reconvergent | 1000000 | 3960000 | edge_insert/append|rebuild | 57,561.5 | 2,539,552.8 | 0.02 | 6% |
| reconvergent | 1000000 | 3960000 | structure MiB | 182.8 | 105.9 | 1.73 |  |

CSR break-even: number of full arrival+required passes after which CSR's (build) cost is repaid:

| topology | nodes | build list | build csr | AT+RT list | AT+RT csr | break-even |
|---|---|---|---|---|---|---|
| layered | 10000 | 4,913.9 | 1,269.3 | 393.8 | 247.7 | already cheaper to build |
| layered | 100000 | 169,147.0 | 91,968.3 | 16,735.4 | 7,610.7 | already cheaper to build |
| layered | 500000 | 1,332,679.1 | 1,076,011.8 | 131,931.6 | 108,042.2 | already cheaper to build |
| layered | 1000000 | 2,921,896.1 | 2,405,679.9 | 302,786.5 | 272,226.2 | already cheaper to build |
| random | 10000 | 3,495.8 | 1,416.1 | 409.4 | 339.9 | already cheaper to build |
| random | 100000 | 169,150.0 | 96,185.7 | 19,050.4 | 8,376.7 | already cheaper to build |
| random | 500000 | 1,345,111.2 | 1,083,960.8 | 152,980.5 | 143,581.4 | already cheaper to build |
| random | 1000000 | 3,122,579.3 | 2,473,195.7 | 377,047.5 | 318,559.9 | already cheaper to build |
| reconvergent | 10000 | 3,897.7 | 1,643.9 | 487.8 | 321.3 | already cheaper to build |
| reconvergent | 100000 | 194,262.1 | 100,511.4 | 19,588.2 | 8,353.2 | already cheaper to build |
| reconvergent | 500000 | 1,258,159.5 | 1,081,392.7 | 135,937.0 | 119,652.4 | already cheaper to build |
| reconvergent | 1000000 | 3,031,089.3 | 2,432,170.5 | 361,541.0 | 296,573.2 | already cheaper to build |

## Experiment B — topology (CSR, serial pull)

| topology | nodes | edges | sources | sinks | max fo | max fi | depth | median level width | arrival µs | edges/s | CV |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layered | 10000 | 39600 | 100 | 284 | 14 | 4 | 100 | 100 | 72.68 | 5.448e+08 | 36.4% |
| layered | 100000 | 396000 | 1000 | 2866 | 14 | 4 | 100 | 1000 | 3,683.1 | 1.075e+08 | 34.5% |
| layered | 500000 | 1980000 | 5000 | 14046 | 16 | 4 | 100 | 5000 | 51,026.1 | 3.880e+07 | 21.4% |
| layered | 1000000 | 3960000 | 10000 | 28140 | 18 | 4 | 100 | 10000 | 133,077.5 | 2.976e+07 | 20.6% |
| random | 10000 | 40000 | 227 | 1965 | 47 | 14 | 60 | 80 | 181.1 | 2.209e+08 | 28.6% |
| random | 100000 | 400000 | 1799 | 20055 | 46 | 16 | 82 | 335 | 4,061.5 | 9.849e+07 | 40.4% |
| random | 500000 | 2000000 | 9177 | 99712 | 52 | 16 | 97 | 1244 | 70,446.3 | 2.839e+07 | 17.3% |
| random | 1000000 | 4000000 | 18228 | 199838 | 57 | 16 | 106 | 1446 | 150,780.2 | 2.653e+07 | 16.1% |
| reconvergent | 10000 | 39600 | 100 | 301 | 15 | 4 | 100 | 100 | 119.3 | 3.319e+08 | 33.6% |
| reconvergent | 100000 | 396000 | 1000 | 2981 | 17 | 4 | 100 | 1000 | 4,282.7 | 9.246e+07 | 68.7% |
| reconvergent | 500000 | 1980000 | 5000 | 15015 | 21 | 4 | 100 | 5000 | 56,801.6 | 3.486e+07 | 18.6% |
| reconvergent | 1000000 | 3960000 | 10000 | 29983 | 19 | 4 | 100 | 10000 | 143,519.9 | 2.759e+07 | 20.5% |

## Experiment C — incremental vs full recompute (CSR)

S_inc = T_full / T_incremental (> 1: incremental faster). affected = nodes in the fanout cone (AT) ∪ fanin cone (RT) of the edited edges; changed = nodes whose AT or RT changed.

_(no data)_

## Experiment D — level-parallel propagation (CSR)

Speedup(p) = T_1 / T_p with T_1 = serial pull AT+RT; efficiency = speedup / p. Algorithm-only = arrival+required; end-to-end adds levelize + pool_start.

_(no data)_

## Experiment E — traversal (CSR)

Traversal cost only; BFS does not produce a timing order in general (column 'BFS order valid'). The last two columns run the same pull propagation over Kahn vs DFS orders.

| topology | nodes | kahn µs | (indegree init) | dfs µs | bfs µs | aux MiB k/d/b | BFS order valid | AT kahn-order | AT dfs-order |
|---|---|---|---|---|---|---|---|---|---|
| layered | 10000 | 303.5 | 15.87 | 352.3 | 283.8 | 0.1/0.0/0.0 | yes | 72.68 | 108.2 |
| layered | 100000 | 6,238.4 | 218.1 | 6,715.2 | 4,523.2 | 0.8/0.5/0.5 | yes | 3,683.1 | 5,367.6 |
| layered | 500000 | 159,564.0 | 1,100.6 | 127,975.1 | 72,208.3 | 3.8/2.4/2.4 | yes | 51,026.1 | 56,276.7 |
| layered | 1000000 | 381,333.6 | 2,281.6 | 305,766.9 | 176,861.6 | 7.6/4.8/4.8 | yes | 133,077.5 | 125,843.3 |
| random | 10000 | 365.3 | 15.24 | 405.2 | 287.8 | 0.1/0.0/0.0 | no | 181.1 | 192.7 |
| random | 100000 | 7,720.3 | 232.8 | 7,257.4 | 4,725.0 | 0.8/0.5/0.5 | no | 4,061.5 | 5,618.9 |
| random | 500000 | 137,341.0 | 1,108.4 | 130,105.5 | 77,908.6 | 3.8/2.4/2.4 | no | 70,446.3 | 74,199.1 |
| random | 1000000 | 360,415.9 | 2,365.1 | 304,466.5 | 182,674.5 | 7.6/4.8/4.8 | no | 150,780.2 | 153,099.5 |
| reconvergent | 10000 | 437.0 | 16.99 | 452.9 | 386.5 | 0.1/0.0/0.0 | no | 119.3 | 219.2 |
| reconvergent | 100000 | 7,313.5 | 206.8 | 7,473.1 | 5,523.7 | 0.8/0.5/0.5 | no | 4,282.7 | 5,944.0 |
| reconvergent | 500000 | 146,945.5 | 1,120.7 | 140,205.5 | 71,764.2 | 3.8/2.4/2.4 | no | 56,801.6 | 56,389.6 |
| reconvergent | 1000000 | 384,198.2 | 2,233.1 | 320,306.6 | 162,465.8 | 7.6/4.8/4.8 | no | 143,519.9 | 152,397.1 |

## Memory and noise

peak_rss_bytes is process-wide (the whole run, incl. reference engine and both representations); structure memory is the computed size of the graph arrays.

| topology | nodes | peak RSS MiB |
|---|---|---|
| layered | 10000 | 16.8 |
| layered | 100000 | 72.1 |
| layered | 500000 | 326.1 |
| layered | 1000000 | 686.9 |
| random | 10000 | 16.8 |
| random | 100000 | 74.8 |
| random | 500000 | 344.2 |
| random | 1000000 | 705.6 |
| reconvergent | 10000 | 16.8 |
| reconvergent | 100000 | 72.0 |
| reconvergent | 500000 | 329.8 |
| reconvergent | 1000000 | 652.7 |

216 configuration(s) with CV > 10% (median > 100 µs): layered/10000/list/build/push_back/t1; layered/10000/list/topo_indegree/kahn/t1; layered/10000/list/topo_sort/kahn/t1; layered/10000/list/topo_sort/dfs/t1; layered/10000/list/traversal/bfs/t1; layered/10000/list/arrival/pull_dfs_order/t1; layered/10000/list/arrival/pull/t1; layered/10000/list/arrival/push/t1; layered/10000/list/required/pull/t1; layered/10000/csr/build/counting_sort/t1; layered/10000/csr/topo_sort/kahn/t1; layered/10000/csr/topo_sort/dfs/t1; layered/10000/csr/traversal/bfs/t1; layered/10000/csr/arrival/pull_dfs_order/t1; layered/10000/csr/arrival/push/t1

