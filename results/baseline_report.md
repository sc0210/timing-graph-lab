# Timing Graph Lab — results summary

- rows: 2664; commits: 43a62d58b247
- CPU: Intel(R) Xeon(R) Processor @ 2.80GHz; build: Release
- seeds: 20261017, 20261018, 20261019; times are medians over repetitions (µs), then median over seeds; `spread` = (max-min)/median of per-seed medians

## Gate D — correctness

All rows passed verification against the reference engine.

Results not bit-identical to the reference (but within tolerance): none

## Experiment A — Adjacency List vs CSR

| topology | nodes | edges | phase | list µs | csr µs | list/csr | seed spread |
|---|---|---|---|---|---|---|---|
| chain | 100000 | 99999 | build/counting_sort | 29,395.6 | 8,422.9 | 3.49 | 21% |
| chain | 100000 | 99999 | topo_sort/kahn | 27,815.4 | 5,181.1 | 5.37 | 28% |
| chain | 100000 | 99999 | arrival/pull | 3,142.7 | 1,116.5 | 2.81 | 25% |
| chain | 100000 | 99999 | arrival/push | 13,769.6 | 2,571.9 | 5.35 | 27% |
| chain | 100000 | 99999 | required/pull | 3,563.7 | 1,229.7 | 2.90 | 29% |
| chain | 100000 | 99999 | edge_insert/append|rebuild | 572.0 | 11,124.3 | 0.05 | 24% |
| chain | 100000 | 99999 | structure MiB | 7.6 | 3.8 | 2.00 |  |
| layered | 100000 | 396000 | build/counting_sort | 164,408.1 | 83,801.6 | 1.96 | 16% |
| layered | 100000 | 396000 | topo_sort/kahn | 41,270.0 | 6,024.1 | 6.85 | 26% |
| layered | 100000 | 396000 | arrival/pull | 7,016.5 | 2,806.4 | 2.50 | 34% |
| layered | 100000 | 396000 | arrival/push | 29,530.5 | 6,472.7 | 4.56 | 16% |
| layered | 100000 | 396000 | required/pull | 8,210.9 | 3,081.6 | 2.66 | 21% |
| layered | 100000 | 396000 | edge_insert/append|rebuild | 2,496.2 | 89,297.7 | 0.03 | 19% |
| layered | 100000 | 396000 | structure MiB | 18.3 | 10.6 | 1.72 |  |
| random | 100000 | 400000 | build/counting_sort | 158,926.7 | 83,812.1 | 1.90 | 9% |
| random | 100000 | 400000 | topo_sort/kahn | 19,710.4 | 7,051.1 | 2.80 | 5% |
| random | 100000 | 400000 | arrival/pull | 8,882.1 | 3,306.7 | 2.69 | 13% |
| random | 100000 | 400000 | arrival/push | 22,297.3 | 6,883.5 | 3.24 | 15% |
| random | 100000 | 400000 | required/pull | 8,274.9 | 3,600.2 | 2.30 | 5% |
| random | 100000 | 400000 | edge_insert/append|rebuild | 1,634.7 | 89,508.9 | 0.02 | 7% |
| random | 100000 | 400000 | structure MiB | 20.3 | 10.7 | 1.90 |  |
| reconvergent | 100000 | 396000 | build/counting_sort | 175,139.7 | 87,670.6 | 2.00 | 13% |
| reconvergent | 100000 | 396000 | topo_sort/kahn | 40,082.4 | 6,553.2 | 6.12 | 4% |
| reconvergent | 100000 | 396000 | arrival/pull | 8,171.9 | 3,416.9 | 2.39 | 30% |
| reconvergent | 100000 | 396000 | arrival/push | 32,638.8 | 7,294.6 | 4.47 | 7% |
| reconvergent | 100000 | 396000 | required/pull | 8,878.1 | 3,517.9 | 2.52 | 25% |
| reconvergent | 100000 | 396000 | edge_insert/append|rebuild | 2,810.2 | 95,111.3 | 0.03 | 10% |
| reconvergent | 100000 | 396000 | structure MiB | 18.3 | 10.6 | 1.73 |  |

CSR break-even: number of full arrival+required passes after which CSR's (build) cost is repaid:

| topology | nodes | build list | build csr | AT+RT list | AT+RT csr | break-even |
|---|---|---|---|---|---|---|
| chain | 100000 | 29,395.6 | 8,422.9 | 6,706.4 | 2,346.2 | already cheaper to build |
| layered | 100000 | 164,408.1 | 83,801.6 | 15,227.4 | 5,888.0 | already cheaper to build |
| random | 100000 | 158,926.7 | 83,812.1 | 17,157.0 | 6,907.0 | already cheaper to build |
| reconvergent | 100000 | 175,139.7 | 87,670.6 | 17,050.0 | 6,934.9 | already cheaper to build |

## Experiment B — topology (CSR, serial pull)

| topology | nodes | edges | sources | sinks | max fo | max fi | depth | median level width | arrival µs | edges/s | CV |
|---|---|---|---|---|---|---|---|---|---|---|---|
| chain | 100000 | 99999 | 1 | 1 | 1 | 1 | 100000 | 1 | 1,116.5 | 8.956e+07 | 33.0% |
| layered | 100000 | 396000 | 1000 | 2866 | 14 | 4 | 100 | 1000 | 2,806.4 | 1.411e+08 | 32.0% |
| random | 100000 | 400000 | 1799 | 20055 | 46 | 16 | 82 | 335 | 3,306.7 | 1.210e+08 | 29.6% |
| reconvergent | 100000 | 396000 | 1000 | 2981 | 17 | 4 | 100 | 1000 | 3,416.9 | 1.159e+08 | 33.2% |

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
| chain | 100000 | 5,181.1 | 197.6 | 4,193.4 | 3,189.4 | 0.8/1.0/0.5 | yes | 1,116.5 | 1,815.6 |
| layered | 100000 | 6,024.1 | 212.1 | 5,888.6 | 4,167.8 | 0.8/0.5/0.5 | yes | 2,806.4 | 4,852.4 |
| random | 100000 | 7,051.1 | 199.5 | 6,724.8 | 4,538.3 | 0.8/0.5/0.5 | no | 3,306.7 | 5,043.4 |
| reconvergent | 100000 | 6,553.2 | 209.7 | 6,920.0 | 4,928.7 | 0.8/0.5/0.5 | no | 3,416.9 | 5,084.0 |

## Memory and noise

peak_rss_bytes is process-wide (the whole run, incl. reference engine and both representations); structure memory is the computed size of the graph arrays.

| topology | nodes | peak RSS MiB |
|---|---|---|
| chain | 100000 | 27.9 |
| layered | 100000 | 71.9 |
| random | 100000 | 74.8 |
| reconvergent | 100000 | 72.1 |

84 configuration(s) with CV > 10% (median > 100 µs): chain/100000/list/build/push_back/t1; chain/100000/list/topo_indegree/kahn/t1; chain/100000/list/topo_sort/kahn/t1; chain/100000/list/topo_sort/dfs/t1; chain/100000/list/traversal/bfs/t1; chain/100000/list/arrival/pull_dfs_order/t1; chain/100000/list/arrival/pull/t1; chain/100000/list/arrival/push/t1; chain/100000/list/required/pull/t1; chain/100000/list/slack/rt_minus_at/t1; chain/100000/list/edge_insert/append/t1; chain/100000/csr/build/counting_sort/t1; chain/100000/csr/topo_indegree/kahn/t1; chain/100000/csr/topo_sort/kahn/t1; chain/100000/csr/topo_sort/dfs/t1

