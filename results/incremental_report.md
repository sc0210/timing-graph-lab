# Timing Graph Lab — results summary

- rows: 4878; commits: 43a62d58b247
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

_(no data)_

## Experiment C — incremental vs full recompute (CSR)

S_inc = T_full / T_incremental (> 1: incremental faster). affected = nodes in the fanout cone (AT) ∪ fanin cone (RT) of the edited edges; changed = nodes whose AT or RT changed.

| topology | nodes | dist | ratio | edges | mode | affected | evaluated | changed | inc µs | full µs | S_inc |
|---|---|---|---|---|---|---|---|---|---|---|---|
| layered | 100000 | critical | 0% | 0 | conservative | 0 | 0 | 0 | 0.84 | 10,349.5 | 12269.71 |
| layered | 100000 | critical | 0% | 0 | early_stop | 0 | 0 | 0 | 0.79 | 10,349.5 | 13117.23 |
| layered | 100000 | distributed | 0% | 0 | conservative | 0 | 0 | 0 | 1.10 | 10,059.9 | 9187.08 |
| layered | 100000 | distributed | 0% | 0 | early_stop | 0 | 0 | 0 | 1.02 | 10,059.9 | 9906.31 |
| layered | 100000 | localized | 0% | 0 | conservative | 0 | 0 | 0 | 0.90 | 10,535.5 | 11693.07 |
| layered | 100000 | localized | 0% | 0 | early_stop | 0 | 0 | 0 | 0.92 | 10,535.5 | 11464.04 |
| layered | 100000 | critical | 0.01% | 40 | conservative | 99867 | 136124 | 86846 | 28,885.5 | 9,431.4 | 0.33 |
| layered | 100000 | critical | 0.01% | 40 | early_stop | 99867 | 106506 | 86846 | 35,222.0 | 9,431.4 | 0.27 |
| layered | 100000 | distributed | 0.01% | 40 | conservative | 99810 | 182277 | 119 | 31,897.5 | 9,518.3 | 0.30 |
| layered | 100000 | distributed | 0.01% | 40 | early_stop | 99810 | 556 | 119 | 200.1 | 9,518.3 | 47.58 |
| layered | 100000 | localized | 0.01% | 40 | conservative | 93055 | 93061 | 163 | 18,706.0 | 10,314.2 | 0.55 |
| layered | 100000 | localized | 0.01% | 40 | early_stop | 93055 | 668 | 163 | 287.2 | 10,314.2 | 35.92 |
| layered | 100000 | critical | 0.1% | 396 | conservative | 99906 | 187963 | 98623 | 37,610.1 | 8,971.5 | 0.24 |
| layered | 100000 | critical | 0.1% | 396 | early_stop | 99906 | 168444 | 98623 | 65,041.9 | 8,971.5 | 0.14 |
| layered | 100000 | distributed | 0.1% | 396 | conservative | 99896 | 187907 | 6915 | 34,490.7 | 9,566.4 | 0.28 |
| layered | 100000 | distributed | 0.1% | 396 | early_stop | 99896 | 24583 | 6915 | 7,687.4 | 9,566.4 | 1.24 |
| layered | 100000 | localized | 0.1% | 396 | conservative | 96225 | 96307 | 1949 | 21,687.2 | 10,759.1 | 0.50 |
| layered | 100000 | localized | 0.1% | 396 | early_stop | 96225 | 6817 | 1949 | 2,219.1 | 10,759.1 | 4.85 |
| layered | 100000 | critical | 1% | 3960 | conservative | 99930 | 192133 | 99859 | 46,630.4 | 9,872.2 | 0.21 |
| layered | 100000 | critical | 1% | 3960 | early_stop | 99930 | 188588 | 99859 | 70,756.8 | 9,872.2 | 0.14 |
| layered | 100000 | distributed | 1% | 3960 | conservative | 99937 | 191370 | 69102 | 42,947.5 | 9,686.5 | 0.23 |
| layered | 100000 | distributed | 1% | 3960 | early_stop | 99937 | 145196 | 69102 | 65,333.2 | 9,686.5 | 0.15 |
| layered | 100000 | localized | 1% | 3960 | conservative | 99142 | 101909 | 42759 | 23,943.3 | 9,624.6 | 0.40 |
| layered | 100000 | localized | 1% | 3960 | early_stop | 99142 | 76011 | 42759 | 30,861.9 | 9,624.6 | 0.31 |
| layered | 100000 | critical | 5% | 19800 | conservative | 99950 | 194339 | 99928 | 51,333.6 | 9,717.9 | 0.19 |
| layered | 100000 | critical | 5% | 19800 | early_stop | 99950 | 193978 | 99928 | 83,395.5 | 9,717.9 | 0.12 |
| layered | 100000 | distributed | 5% | 19800 | conservative | 99950 | 193650 | 99758 | 56,631.8 | 10,094.9 | 0.18 |
| layered | 100000 | distributed | 5% | 19800 | early_stop | 99950 | 189307 | 99758 | 81,394.3 | 10,094.9 | 0.12 |
| layered | 100000 | localized | 5% | 19800 | conservative | 99527 | 106249 | 99167 | 41,679.1 | 11,476.9 | 0.28 |
| layered | 100000 | localized | 5% | 19800 | early_stop | 99527 | 105884 | 99167 | 55,814.8 | 11,476.9 | 0.21 |
| layered | 100000 | critical | 10% | 39600 | conservative | 99956 | 195082 | 99938 | 59,573.0 | 9,355.1 | 0.16 |
| layered | 100000 | critical | 10% | 39600 | early_stop | 99956 | 195034 | 99938 | 105,037.9 | 9,355.1 | 0.09 |
| layered | 100000 | distributed | 10% | 39600 | conservative | 99967 | 194427 | 99856 | 59,538.2 | 8,937.9 | 0.15 |
| layered | 100000 | distributed | 10% | 39600 | early_stop | 99967 | 192760 | 99856 | 100,927.3 | 8,937.9 | 0.09 |
| layered | 100000 | localized | 10% | 39600 | conservative | 99151 | 108709 | 99140 | 35,679.8 | 9,058.8 | 0.25 |
| layered | 100000 | localized | 10% | 39600 | early_stop | 99151 | 108696 | 99140 | 57,930.2 | 9,058.8 | 0.16 |
| random | 100000 | critical | 0% | 0 | conservative | 0 | 0 | 0 | 0.97 | 9,791.2 | 10068.11 |
| random | 100000 | critical | 0% | 0 | early_stop | 0 | 0 | 0 | 1.13 | 9,791.2 | 8676.33 |
| random | 100000 | distributed | 0% | 0 | conservative | 0 | 0 | 0 | 0.92 | 11,372.6 | 12328.01 |
| random | 100000 | distributed | 0% | 0 | early_stop | 0 | 0 | 0 | 1.01 | 11,372.6 | 11287.93 |
| random | 100000 | localized | 0% | 0 | conservative | 0 | 0 | 0 | 1.02 | 9,968.7 | 9735.08 |
| random | 100000 | localized | 0% | 0 | early_stop | 0 | 0 | 0 | 0.98 | 9,968.7 | 10208.63 |
| random | 100000 | critical | 0.01% | 40 | conservative | 98229 | 108732 | 98065 | 26,913.2 | 9,487.6 | 0.35 |
| random | 100000 | critical | 0.01% | 40 | early_stop | 98229 | 102974 | 98065 | 40,887.1 | 9,487.6 | 0.23 |
| random | 100000 | distributed | 0.01% | 40 | conservative | 89358 | 90711 | 26 | 22,611.4 | 10,711.7 | 0.47 |
| random | 100000 | distributed | 0.01% | 40 | early_stop | 89358 | 182 | 26 | 138.2 | 10,711.7 | 77.52 |
| random | 100000 | localized | 0.01% | 40 | conservative | 71628 | 71631 | 393 | 15,765.8 | 9,808.3 | 0.62 |
| random | 100000 | localized | 0.01% | 40 | early_stop | 71628 | 2133 | 393 | 911.9 | 9,808.3 | 10.76 |
| random | 100000 | critical | 0.1% | 400 | conservative | 98441 | 117802 | 98162 | 31,424.0 | 9,784.1 | 0.31 |
| random | 100000 | critical | 0.1% | 400 | early_stop | 98441 | 109518 | 98162 | 47,503.7 | 9,784.1 | 0.21 |
| random | 100000 | distributed | 0.1% | 400 | conservative | 97639 | 112106 | 1810 | 31,675.0 | 11,586.9 | 0.37 |
| random | 100000 | distributed | 0.1% | 400 | early_stop | 97639 | 6419 | 1810 | 3,621.7 | 11,586.9 | 3.20 |
| random | 100000 | localized | 0.1% | 400 | conservative | 60052 | 60484 | 1485 | 14,955.3 | 9,886.6 | 0.66 |
| random | 100000 | localized | 0.1% | 400 | early_stop | 60052 | 4293 | 1485 | 1,563.3 | 9,886.6 | 6.32 |
| random | 100000 | critical | 1% | 4000 | conservative | 98815 | 136885 | 98586 | 35,883.9 | 10,153.4 | 0.28 |
| random | 100000 | critical | 1% | 4000 | early_stop | 98815 | 131247 | 98586 | 63,694.0 | 10,153.4 | 0.16 |
| random | 100000 | distributed | 1% | 4000 | conservative | 98711 | 132062 | 33803 | 39,697.6 | 11,073.4 | 0.28 |
| random | 100000 | distributed | 1% | 4000 | early_stop | 98711 | 77330 | 33803 | 39,211.4 | 11,073.4 | 0.28 |
| random | 100000 | localized | 1% | 4000 | conservative | 98485 | 121354 | 14608 | 31,554.3 | 10,522.7 | 0.33 |
| random | 100000 | localized | 1% | 4000 | early_stop | 98485 | 52087 | 14608 | 23,437.5 | 10,522.7 | 0.45 |
| random | 100000 | critical | 5% | 20000 | conservative | 99086 | 154861 | 98976 | 55,577.3 | 14,496.2 | 0.26 |
| random | 100000 | critical | 5% | 20000 | early_stop | 99086 | 153377 | 98976 | 78,258.6 | 14,496.2 | 0.19 |
| random | 100000 | distributed | 5% | 20000 | conservative | 99006 | 146305 | 98479 | 50,299.4 | 10,139.0 | 0.20 |
| random | 100000 | distributed | 5% | 20000 | early_stop | 99006 | 137720 | 98479 | 68,382.2 | 10,139.0 | 0.15 |
| random | 100000 | localized | 5% | 20000 | conservative | 98677 | 130244 | 98239 | 39,517.0 | 9,610.3 | 0.24 |
| random | 100000 | localized | 5% | 20000 | early_stop | 98677 | 117768 | 98239 | 64,613.5 | 9,610.3 | 0.15 |
| random | 100000 | critical | 10% | 40000 | conservative | 99195 | 162442 | 99137 | 64,254.8 | 11,160.0 | 0.17 |
| random | 100000 | critical | 10% | 40000 | early_stop | 99195 | 161835 | 99137 | 86,937.0 | 11,160.0 | 0.13 |
| random | 100000 | distributed | 10% | 40000 | conservative | 99149 | 154160 | 98774 | 59,809.9 | 10,396.7 | 0.17 |
| random | 100000 | distributed | 10% | 40000 | early_stop | 99149 | 149170 | 98774 | 86,698.4 | 10,396.7 | 0.12 |
| random | 100000 | localized | 10% | 40000 | conservative | 98797 | 136435 | 98375 | 59,788.1 | 11,216.1 | 0.19 |
| random | 100000 | localized | 10% | 40000 | early_stop | 98797 | 126561 | 98375 | 83,961.9 | 11,216.1 | 0.13 |
| reconvergent | 100000 | critical | 0% | 0 | conservative | 0 | 0 | 0 | 0.71 | 9,949.2 | 13944.23 |
| reconvergent | 100000 | critical | 0% | 0 | early_stop | 0 | 0 | 0 | 0.68 | 9,949.2 | 14566.93 |
| reconvergent | 100000 | distributed | 0% | 0 | conservative | 0 | 0 | 0 | 0.97 | 10,519.8 | 10806.17 |
| reconvergent | 100000 | distributed | 0% | 0 | early_stop | 0 | 0 | 0 | 0.94 | 10,519.8 | 11215.14 |
| reconvergent | 100000 | localized | 0% | 0 | conservative | 0 | 0 | 0 | 0.82 | 9,831.1 | 11952.74 |
| reconvergent | 100000 | localized | 0% | 0 | early_stop | 0 | 0 | 0 | 1.17 | 9,831.1 | 8381.18 |
| reconvergent | 100000 | critical | 0.01% | 40 | conservative | 99605 | 165319 | 81745 | 44,163.2 | 10,079.6 | 0.23 |
| reconvergent | 100000 | critical | 0.01% | 40 | early_stop | 99605 | 136503 | 81745 | 52,662.2 | 10,079.6 | 0.19 |
| reconvergent | 100000 | distributed | 0.01% | 40 | conservative | 99815 | 175960 | 272 | 37,206.6 | 10,007.7 | 0.27 |
| reconvergent | 100000 | distributed | 0.01% | 40 | early_stop | 99815 | 1107 | 272 | 498.6 | 10,007.7 | 20.07 |
| reconvergent | 100000 | localized | 0.01% | 40 | conservative | 91992 | 92008 | 203 | 21,272.2 | 10,340.6 | 0.49 |
| reconvergent | 100000 | localized | 0.01% | 40 | early_stop | 91992 | 826 | 203 | 394.9 | 10,340.6 | 26.18 |
| reconvergent | 100000 | critical | 0.1% | 396 | conservative | 99920 | 185323 | 98993 | 42,487.1 | 10,006.4 | 0.24 |
| reconvergent | 100000 | critical | 0.1% | 396 | early_stop | 99920 | 168334 | 98993 | 70,558.9 | 10,006.4 | 0.14 |
| reconvergent | 100000 | distributed | 0.1% | 396 | conservative | 99897 | 182665 | 3355 | 35,930.2 | 10,334.3 | 0.29 |
| reconvergent | 100000 | distributed | 0.1% | 396 | early_stop | 99897 | 12792 | 3355 | 4,782.5 | 10,334.3 | 2.16 |
| reconvergent | 100000 | localized | 0.1% | 396 | conservative | 98016 | 98654 | 3801 | 21,584.7 | 11,095.7 | 0.51 |
| reconvergent | 100000 | localized | 0.1% | 396 | early_stop | 98016 | 13527 | 3801 | 5,201.0 | 11,095.7 | 2.13 |
| reconvergent | 100000 | critical | 1% | 3960 | conservative | 99975 | 190851 | 99884 | 45,902.2 | 10,582.1 | 0.23 |
| reconvergent | 100000 | critical | 1% | 3960 | early_stop | 99975 | 188266 | 99884 | 76,998.5 | 10,582.1 | 0.14 |
| reconvergent | 100000 | distributed | 1% | 3960 | conservative | 99962 | 188345 | 94246 | 42,532.6 | 9,632.0 | 0.23 |
| reconvergent | 100000 | distributed | 1% | 3960 | early_stop | 99962 | 161035 | 94246 | 67,867.4 | 9,632.0 | 0.14 |
| reconvergent | 100000 | localized | 1% | 3960 | conservative | 98273 | 105453 | 34622 | 25,270.6 | 10,026.6 | 0.40 |
| reconvergent | 100000 | localized | 1% | 3960 | early_stop | 98273 | 69800 | 34622 | 32,387.7 | 10,026.6 | 0.31 |
| reconvergent | 100000 | critical | 5% | 19800 | conservative | 99988 | 193850 | 99973 | 55,943.2 | 9,754.5 | 0.17 |
| reconvergent | 100000 | critical | 5% | 19800 | early_stop | 99988 | 193617 | 99973 | 95,566.3 | 9,754.5 | 0.10 |
| reconvergent | 100000 | distributed | 5% | 19800 | conservative | 99982 | 191851 | 99722 | 62,967.9 | 11,814.7 | 0.19 |
| reconvergent | 100000 | distributed | 5% | 19800 | early_stop | 99982 | 185870 | 99722 | 91,251.6 | 11,814.7 | 0.13 |
| reconvergent | 100000 | localized | 5% | 19800 | conservative | 98522 | 115004 | 97488 | 37,642.0 | 10,204.9 | 0.27 |
| reconvergent | 100000 | localized | 5% | 19800 | early_stop | 98522 | 110681 | 97488 | 59,131.1 | 10,204.9 | 0.17 |
| reconvergent | 100000 | critical | 10% | 39600 | conservative | 99989 | 194864 | 99983 | 69,425.3 | 9,904.6 | 0.14 |
| reconvergent | 100000 | critical | 10% | 39600 | early_stop | 99989 | 194820 | 99983 | 111,448.6 | 9,904.6 | 0.09 |
| reconvergent | 100000 | distributed | 10% | 39600 | conservative | 99987 | 193204 | 99903 | 63,445.6 | 9,384.8 | 0.15 |
| reconvergent | 100000 | distributed | 10% | 39600 | early_stop | 99987 | 191065 | 99903 | 104,832.3 | 9,384.8 | 0.09 |
| reconvergent | 100000 | localized | 10% | 39600 | conservative | 99154 | 118219 | 99071 | 42,515.1 | 10,008.2 | 0.24 |
| reconvergent | 100000 | localized | 10% | 39600 | early_stop | 99154 | 115803 | 99071 | 68,424.0 | 10,008.2 | 0.15 |

## Experiment D — level-parallel propagation (CSR)

Speedup(p) = T_1 / T_p with T_1 = serial pull AT+RT; efficiency = speedup / p. Algorithm-only = arrival+required; end-to-end adds levelize + pool_start.

_(no data)_

## Experiment E — traversal (CSR)

Traversal cost only; BFS does not produce a timing order in general (column 'BFS order valid'). The last two columns run the same pull propagation over Kahn vs DFS orders.

_(no data)_

## Memory and noise

peak_rss_bytes is process-wide (the whole run, incl. reference engine and both representations); structure memory is the computed size of the graph arrays.

| topology | nodes | peak RSS MiB |
|---|---|---|
| layered | 100000 | 56.8 |
| random | 100000 | 59.2 |
| reconvergent | 100000 | 56.2 |

139 configuration(s) with CV > 10% (median > 100 µs): layered/100000/csr/full_recompute/pull/t1; layered/100000/csr/incremental/conservative/t1; layered/100000/csr/full_recompute/pull/t1; layered/100000/csr/incremental/early_stop/t1; layered/100000/csr/incremental/conservative/t1; layered/100000/csr/full_recompute/pull/t1; layered/100000/csr/incremental/early_stop/t1; layered/100000/csr/incremental/conservative/t1; layered/100000/csr/full_recompute/pull/t1; layered/100000/csr/incremental/early_stop/t1; layered/100000/csr/incremental/conservative/t1; layered/100000/csr/full_recompute/pull/t1; layered/100000/csr/incremental/early_stop/t1; layered/100000/csr/incremental/conservative/t1; layered/100000/csr/full_recompute/pull/t1

